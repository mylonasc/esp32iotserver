from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, Iterable, List, Optional

import requests

from opentelemetry.metrics import Observation
from opentelemetry.sdk.metrics import MeterProvider
from opentelemetry.sdk.metrics.export import PeriodicExportingMetricReader
from opentelemetry.exporter.otlp.proto.http.metric_exporter import (
    OTLPMetricExporter,
)


@dataclass(frozen=True)
class MetricSample:
    name: str
    value: float
    attributes: Dict[str, str]


@dataclass
class ScrapeResult:
    device_id: str
    samples: List[MetricSample]
    errors: List[str]


class ModuleParser:
    module_name: str

    def is_active(
        self, api_summary: Dict[str, Any], module_json: Optional[Dict[str, Any]]
    ) -> bool:
        return module_json is not None

    def parse(self, device_id: str, module_json: Dict[str, Any]) -> List[MetricSample]:
        return []


class DhtParser(ModuleParser):
    module_name = "dht"

    def is_active(
        self, api_summary: Dict[str, Any], module_json: Optional[Dict[str, Any]]
    ) -> bool:
        if not module_json:
            return False
        return bool(
            module_json.get("enabled")
            or module_json.get("running")
            or module_json.get("hasValue")
        )

    def parse(self, device_id: str, module_json: Dict[str, Any]) -> List[MetricSample]:
        samples: List[MetricSample] = []
        attrs = {"device": device_id, "sensor_id": str(module_json.get("id", "dht"))}
        if module_json.get("hasValue"):
            samples.append(
                MetricSample(
                    "esp32_dht_temperature_c",
                    float(module_json.get("temperatureC", 0.0)),
                    attrs,
                )
            )
            samples.append(
                MetricSample(
                    "esp32_dht_humidity_percent",
                    float(module_json.get("humidity", 0.0)),
                    attrs,
                )
            )
        return samples


class SoilParser(ModuleParser):
    module_name = "soil"

    def is_active(
        self, api_summary: Dict[str, Any], module_json: Optional[Dict[str, Any]]
    ) -> bool:
        soil_summary = api_summary.get("modules", {}).get("soil", {})
        return soil_summary.get("enabledCount", 0) > 0

    def parse(self, device_id: str, module_json: Dict[str, Any]) -> List[MetricSample]:
        samples: List[MetricSample] = []
        sensors = module_json.get("sensors", [])
        for sensor in sensors:
            if not sensor.get("enabled"):
                continue
            attrs = {
                "device": device_id,
                "sensor_id": str(sensor.get("id", "soil")),
            }
            if sensor.get("hasValue"):
                samples.append(
                    MetricSample(
                        "esp32_soil_moisture_percent",
                        float(sensor.get("percent", 0.0)),
                        attrs,
                    )
                )
                samples.append(
                    MetricSample(
                        "esp32_soil_raw",
                        float(sensor.get("raw", 0.0)),
                        attrs,
                    )
                )
        return samples


class RelayParser(ModuleParser):
    module_name = "relays"

    def is_active(
        self, api_summary: Dict[str, Any], module_json: Optional[Dict[str, Any]]
    ) -> bool:
        if not module_json:
            return False
        relays = module_json.get("relays", [])
        return any(relay.get("enabled") for relay in relays)

    def parse(self, device_id: str, module_json: Dict[str, Any]) -> List[MetricSample]:
        samples: List[MetricSample] = []
        for idx, relay in enumerate(module_json.get("relays", [])):
            if not relay.get("enabled"):
                continue
            attrs = {
                "device": device_id,
                "relay_id": str(relay.get("id", f"relay_{idx}")),
                "index": str(idx),
            }
            samples.append(
                MetricSample(
                    "esp32_relay_state",
                    1.0 if relay.get("on") else 0.0,
                    attrs,
                )
            )
        return samples


class PumpParser(ModuleParser):
    module_name = "pumps"

    def is_active(
        self, api_summary: Dict[str, Any], module_json: Optional[Dict[str, Any]]
    ) -> bool:
        return bool(
            module_json
            and (
                module_json.get("running") or module_json.get("remainingSeconds", 0) > 0
            )
        )

    def parse(self, device_id: str, module_json: Dict[str, Any]) -> List[MetricSample]:
        samples: List[MetricSample] = []
        attrs = {"device": device_id}
        samples.append(
            MetricSample(
                "esp32_pumps_running",
                1.0 if module_json.get("running") else 0.0,
                attrs,
            )
        )
        samples.append(
            MetricSample(
                "esp32_pumps_remaining_seconds",
                float(module_json.get("remainingSeconds", 0.0)),
                attrs,
            )
        )
        return samples


MODULE_PARSERS: Dict[str, ModuleParser] = {
    "dht": DhtParser(),
    "soil": SoilParser(),
    "relays": RelayParser(),
    "pumps": PumpParser(),
}


class Esp32Device:
    def __init__(
        self,
        host: str,
        timeout_s: float = 3.0,
        session: Optional[requests.Session] = None,
    ) -> None:
        if host.startswith("http://") or host.startswith("https://"):
            self.base_url = host.rstrip("/")
        else:
            self.base_url = f"http://{host}"
        self.timeout_s = timeout_s
        self.session = session or requests.Session()

    def _get_json(self, path: str) -> Dict[str, Any]:
        url = f"{self.base_url}{path}"
        resp = self.session.get(url, timeout=self.timeout_s)
        resp.raise_for_status()
        return resp.json()

    def _discover_module_endpoints(self, api_summary: Dict[str, Any]) -> Dict[str, str]:
        endpoints: Dict[str, str] = {}
        try:
            registry = self._get_json("/api/modules")
            for module in registry.get("modules", []):
                name = module.get("name")
                api = module.get("api")
                if name and api:
                    endpoints[name] = api
        except Exception:
            pass

        if not endpoints:
            modules = api_summary.get("modules", {})
            for name, info in modules.items():
                api = info.get("api")
                if api:
                    endpoints[name] = api

        return endpoints

    def scrape(self) -> ScrapeResult:
        errors: List[str] = []
        samples: List[MetricSample] = []

        try:
            api_summary = self._get_json("/api")
        except Exception as exc:
            return ScrapeResult(self.base_url, [], [f"/api failed: {exc}"])

        device_id = str(
            api_summary.get("hostname") or api_summary.get("ip") or self.base_url
        )

        try:
            uptime = float(api_summary.get("uptime_seconds", 0.0))
            wifi = api_summary.get("wifi_status") == "connected"
            samples.append(
                MetricSample("esp32_uptime_seconds", uptime, {"device": device_id})
            )
            samples.append(
                MetricSample(
                    "esp32_wifi_connected", 1.0 if wifi else 0.0, {"device": device_id}
                )
            )
        except Exception as exc:
            errors.append(f"core metric parse failed: {exc}")

        endpoints = self._discover_module_endpoints(api_summary)

        for module_name, api_path in endpoints.items():
            parser = MODULE_PARSERS.get(module_name)
            if not parser:
                continue
            try:
                module_json = self._get_json(api_path)
            except Exception as exc:
                errors.append(f"{module_name} fetch failed: {exc}")
                continue

            if not parser.is_active(api_summary, module_json):
                continue

            try:
                samples.extend(parser.parse(device_id, module_json))
            except Exception as exc:
                errors.append(f"{module_name} parse failed: {exc}")

        return ScrapeResult(device_id, samples, errors)

    def send_to_otel(
        self, otel_endpoint: str, headers: Optional[Dict[str, str]] = None
    ) -> ScrapeResult:
        result = self.scrape()
        if not result.samples:
            return result

        exporter = OTLPMetricExporter(endpoint=otel_endpoint, headers=headers or {})
        reader = PeriodicExportingMetricReader(exporter)
        provider = MeterProvider(metric_readers=[reader])
        meter = provider.get_meter("esp32.scraper")

        by_name: Dict[str, List[MetricSample]] = {}
        for sample in result.samples:
            by_name.setdefault(sample.name, []).append(sample)

        for name, samples in by_name.items():

            def _make_cb(samples_for_metric: List[MetricSample]):
                def cb(options):
                    return [
                        Observation(s.value, s.attributes) for s in samples_for_metric
                    ]

                return cb

            meter.create_observable_gauge(
                name=name,
                callbacks=[_make_cb(samples)],
            )

        provider.force_flush()
        provider.shutdown()
        return result


class Esp32Fleet:
    def __init__(self, hosts: Iterable[str], timeout_s: float = 3.0) -> None:
        self.devices = [Esp32Device(host, timeout_s=timeout_s) for host in hosts]

    def scrape_all(self) -> List[ScrapeResult]:
        return [device.scrape() for device in self.devices]

    def send_all_to_otel(
        self, otel_endpoint: str, headers: Optional[Dict[str, str]] = None
    ) -> List[ScrapeResult]:
        return [
            device.send_to_otel(otel_endpoint, headers=headers)
            for device in self.devices
        ]
