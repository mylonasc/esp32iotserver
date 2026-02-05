from __future__ import annotations

from esp32_scraper import Esp32Fleet


DEVICES = [
    "http://esp32-plants.local",
    "http://esp32-newname.local",
    "http://esp32-humidity.local",
]

OTEL_SERVER_URL = "http://192.168.178.36:4318/v1/metrics"


def main() -> None:
    fleet = Esp32Fleet(DEVICES)
    results = fleet.send_all_to_otel(OTEL_SERVER_URL)

    for result in results:
        status = "ok" if not result.errors else "errors"
        print(f"{result.device_id}: {status}, samples={len(result.samples)}")
        for err in result.errors:
            print(f"  - {err}")


if __name__ == "__main__":
    main()
