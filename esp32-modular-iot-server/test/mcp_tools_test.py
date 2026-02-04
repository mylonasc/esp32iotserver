#!/usr/bin/env python3
import argparse
import json
import sys
import time
import urllib.request


def http_post_json(url, payload, timeout=5):
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        raw = resp.read().decode("utf-8")
    try:
        return json.loads(raw)
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"Invalid JSON response: {raw}") from exc


def http_get_text(url, timeout=5):
    with urllib.request.urlopen(url, timeout=timeout) as resp:
        return resp.read().decode("utf-8")


def mcp_call(base_url, name, arguments=None, request_id=1):
    payload = {
        "jsonrpc": "2.0",
        "id": request_id,
        "method": "tools/call",
        "params": {
            "name": name,
            "arguments": arguments or {},
        },
    }
    resp = http_post_json(f"{base_url}/mcp", payload)
    if "error" in resp:
        raise RuntimeError(f"MCP error: {resp['error']}")
    content = resp.get("result", {}).get("content", [])
    if not content:
        raise RuntimeError("MCP response missing content")
    return content[0].get("json")


def mcp_list(base_url):
    payload = {"jsonrpc": "2.0", "id": 1, "method": "tools/list"}
    resp = http_post_json(f"{base_url}/mcp", payload)
    if "error" in resp:
        raise RuntimeError(f"MCP error: {resp['error']}")
    return resp.get("result", {}).get("tools", [])


def assert_in_ui(html, needle):
    if needle not in html:
        raise AssertionError(f"Expected to find in UI: {needle}")


def main():
    parser = argparse.ArgumentParser(description="Basic MCP tools smoke test")
    parser.add_argument(
        "--base-url",
        default="http://esp32.local",
        help="Base URL for the ESP32 web server",
    )
    parser.add_argument(
        "--check-ui",
        action="store_true",
        help="Also fetch /config and verify values appear in HTML",
    )
    args = parser.parse_args()

    base_url = args.base_url.rstrip("/")

    print(f"Using base URL: {base_url}")

    tools = mcp_list(base_url)
    tool_names = {t.get("name") for t in tools}
    required = {
        "core.info",
        "modules.list",
        "modules.status",
        "pumps.status",
        "pumps.config.get",
        "pumps.config.set",
        "soil.readings",
        "soil.config.get",
        "soil.config.set",
    }
    missing = required - tool_names
    if missing:
        raise RuntimeError(f"Missing MCP tools: {sorted(missing)}")
    print("MCP tools list OK")

    core = mcp_call(base_url, "core.info", request_id=2)
    print("core.info:", core)

    modules = mcp_call(base_url, "modules.list", request_id=3)
    print("modules.list:", modules)

    status = mcp_call(base_url, "modules.status", request_id=4)
    print("modules.status:", status)

    # Pump config roundtrip
    pumps_cfg = mcp_call(base_url, "pumps.config.get", request_id=5)
    print("pumps.config.get:", pumps_cfg)

    original_enabled_a = bool(pumps_cfg.get("enabledA", True))
    toggled_enabled_a = not original_enabled_a

    # Soil config roundtrip (capture originals for restore)
    soil_cfg = mcp_call(base_url, "soil.config.get", request_id=8)
    print("soil.config.get:", soil_cfg)
    original_interval = int(soil_cfg.get("intervalMs", 300))
    new_interval = 500 if original_interval != 500 else 600

    try:
        mcp_call(
            base_url,
            "pumps.config.set",
            {
                "enabledA": toggled_enabled_a,
                "apply": True,
                "save": True,
            },
            request_id=6,
        )
        time.sleep(0.2)

        pumps_cfg_after = mcp_call(base_url, "pumps.config.get", request_id=7)
        if bool(pumps_cfg_after.get("enabledA", True)) != toggled_enabled_a:
            raise AssertionError("pumps.config.set did not apply enabledA")
        print("pumps.config.set OK")

        mcp_call(
            base_url,
            "soil.config.set",
            {
                "intervalMs": new_interval,
                "apply": True,
                "save": True,
            },
            request_id=9,
        )
        time.sleep(0.2)
        soil_cfg_after = mcp_call(base_url, "soil.config.get", request_id=10)
        if int(soil_cfg_after.get("intervalMs", 0)) != new_interval:
            raise AssertionError("soil.config.set did not apply intervalMs")
        print("soil.config.set OK")

        soil_readings = mcp_call(base_url, "soil.readings", request_id=11)
        print("soil.readings:", soil_readings)

        if args.check_ui:
            html = http_get_text(f"{base_url}/config")
            assert_in_ui(html, f"name='soil_int' min='50' value='{new_interval}'")
            # checkbox checked if enabledA; this is simple substring check
            if toggled_enabled_a:
                assert_in_ui(html, "name='pumpA_en' checked")
            print("/config UI check OK")
    finally:
        # Restore original settings even on failure
        mcp_call(
            base_url,
            "pumps.config.set",
            {"enabledA": original_enabled_a, "apply": True, "save": True},
            request_id=12,
        )
        mcp_call(
            base_url,
            "soil.config.set",
            {"intervalMs": original_interval, "apply": True, "save": True},
            request_id=13,
        )
        print("Restored original settings")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)
