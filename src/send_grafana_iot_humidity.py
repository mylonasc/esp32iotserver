import requests
import re
import time
import json

# ==============================================================================
# --- 1. CENTRALIZED CONFIGURATION ---
# ==============================================================================
# Define all your IoT devices here. Add or remove as needed.

DEVICES = [
    {
        "url": "http://esp32-plants.local",  # Base URL for the legacy device
        "type": "legacy_html",
        "description": "Legacy plant sensor scraping from HTML.",
        "common_attributes": {
            "device_id": "esp32-plants.local-humidity",
            "location": "Winter balcony",
            "soil_type": "bad, totally packed with roots"
        }
    },
    {
        "url": "http://esp32-newname.local",  # Base URL for the new device with JSON API
        "type": "smooth_api",
        "description": "New plant sensor using smooth reading JSON API.",
        "common_attributes": {
            "device_id": "esp32-newname.local-smooth-humidity",
            "location": "Living room",
            "soil_type": "no idea - whatever came with the plant."
        }
    }
]

# URL of the OpenTelemetry Collector
OTEL_SERVER_URL = "http://192.168.178.36:4318/v1/metrics"
POLL_INTERVAL_SECONDS = 2 # For polling the smooth_api device


# ==============================================================================
# --- 2. HELPER CLASS FOR NEW API DEVICE ---
# ==============================================================================
class ESPSensorReader:
    """A client to interact with the ESP32's smooth reading JSON API."""
    def __init__(self, base_url: str):
        self.base_url = base_url.rstrip('/')
        self.api_url = f"{self.base_url}/api"
        self.smooth_read_url = f"{self.base_url}/api/smooth_humi/read"

    def get_available_sensors(self) -> list:
        try:
            response = requests.get(self.api_url, timeout=5)
            response.raise_for_status()
            data = response.json()
            if data and "enabled_sensors" in data:
                return [sensor['label'] for sensor in data['enabled_sensors']]
        except requests.exceptions.RequestException as e:
            print(f"❌ Error getting sensors from {self.base_url}: {e}")
        return []

    def request_smooth_reading(self, sensor_label: str) -> dict:
        print(f"▶️  Requesting smooth reading for '{sensor_label}' at {self.base_url}...")
        payload = {"sensor_label": sensor_label}
        try:
            response = requests.post(
                self.smooth_read_url, data=json.dumps(payload),
                headers={'Content-Type': 'application/json'}, timeout=5
            )
            if response.status_code != 202:
                print(f"   ❌ Error starting task: {response.status_code} - {response.text}")
                return None
            print("   ✅ Task started. Polling for result...")
            while True:
                time.sleep(POLL_INTERVAL_SECONDS)
                status_response = requests.get(self.api_url, timeout=5)
                status_response.raise_for_status()
                status = status_response.json()
                if not status.get('smooth_humi_task_running', False):
                    print("   🎉 Task finished!")
                    return status.get('last_smooth_humi_result')
        except requests.exceptions.RequestException as e:
            print(f"   ❌ An error occurred during request for '{sensor_label}': {e}")
            return None


# ==============================================================================
# --- 3. DATA FETCHER FUNCTIONS ---
# ==============================================================================
# Each function fetches data and returns it in a standard format:
# A list of dictionaries, where each dict is a single measurement.

def fetch_from_legacy_html_device(config: dict) -> list:
    """Fetches data by scraping an HTML page."""
    print(f"\nFetching from LEGACY device: {config['url']}")
    measurements = []
    url = f"{config['url']}/watering_pumps"
    try:
        response = requests.get(url, timeout=10)
        response.raise_for_status()
        
        # Use regex to find sensor values
        found = re.search(r"Humidity A: (\d+)% Humidity B: (\d+)%", response.content.decode())
        if not found:
            print(f"  Could not parse humidity values from {url}")
            return []
            
        humidity_values = found.groups()
        sensor_data = {k: v for k, v in zip(['Sensor A', 'Sensor B'], humidity_values)}
        
        print(f"  Successfully parsed data: {sensor_data}")
        
        # Convert to standard measurement format
        for sensor_id, value in sensor_data.items():
            attributes = {**config.get("common_attributes", {}), "sensor_id": sensor_id}
            measurements.append({"value": float(value), "attributes": attributes})
            
    except requests.exceptions.RequestException as e:
        print(f"  Error fetching data from {url}: {e}")
    return measurements

def fetch_from_smooth_api_device(config: dict) -> list:
    """Fetches data using the smooth reading JSON API."""
    print(f"\nFetching from SMOOTH API device: {config['url']}")
    measurements = []
    reader = ESPSensorReader(config['url'])
    
    # Discover available sensors on this device
    sensor_labels = reader.get_available_sensors()
    if not sensor_labels:
        print(f"  No enabled sensors found on {config['url']}.")
        return []
        
    print(f"  Found sensors: {', '.join(sensor_labels)}")
    
    # Get a smooth reading for each sensor
    for label in sensor_labels:
        result = reader.request_smooth_reading(label)
        if result and result.get('sensor_label') == label:
            attributes = {**config.get("common_attributes", {}), "sensor_id": label}
            value = result.get('average_humidity_percent')
            measurements.append({"value": float(value), "attributes": attributes})
            
    return measurements


# ==============================================================================
# --- 4. OPENTELEMETRY FORMATTING AND SENDING ---
# ==============================================================================
# This section is mostly unchanged from your original code.

def _format_for_otel(metric_name: str, unit: str, all_measurements: list[dict]) -> dict:
    """Formats a list of measurements into a single OpenTelemetry metric message."""
    otel_data_points = []
    current_timestamp_nano = int(time.time() * 1_000_000_000)

    for measurement in all_measurements:
        attributes = measurement.get('attributes', {})
        otel_attributes = []
        for key, val in attributes.items():
            if isinstance(val, str):
                otel_attributes.append({"key": key, "value": {"stringValue": val}})
            elif isinstance(val, (int, float)):
                otel_attributes.append({"key": key, "value": {"doubleValue": float(val)}})
            elif isinstance(val, bool):
                otel_attributes.append({"key": key, "value": {"boolValue": val}})
        
        otel_data_points.append({
            "startTimeUnixNano": current_timestamp_nano,
            "timeUnixNano": current_timestamp_nano,
            "asDouble": float(measurement.get('value')),
            "attributes": otel_attributes
        })

    if not otel_data_points:
        return None

    return {
        "resourceMetrics": [{
            "resource": {"attributes": [{"key": "service.name", "value": {"stringValue": "multi-device-plant-monitor"}}]},
            "scopeMetrics": [{
                "scope": {"name": "iot-soil-monitoring", "version": "1.1.0"},
                "metrics": [{
                    "name": metric_name, "description": "Soil moisture measurements from multiple IoT devices",
                    "unit": unit,
                    "gauge": {"dataPoints": otel_data_points}
                }]
            }]
        }]
    }

# ==============================================================================
# --- 5. MAIN EXECUTION LOGIC ---
# ==============================================================================

def collect_and_send_metrics():
    """Main function to collect data from all configured devices and send to OTel."""
    all_measurements = []

    # A mapping from device type to the function that handles it
    FETCHER_MAP = {
        "legacy_html": fetch_from_legacy_html_device,
        "smooth_api": fetch_from_smooth_api_device,
    }

    # Loop through each configured device and collect data
    for device_config in DEVICES:
        device_type = device_config.get("type")
        fetcher_function = FETCHER_MAP.get(device_type)
        
        if fetcher_function:
            measurements = fetcher_function(device_config)
            all_measurements.extend(measurements)
        else:
            print(f"Warning: Unknown device type '{device_type}' for URL {device_config['url']}. Skipping.")
    
    if not all_measurements:
        print("\nNo measurements collected from any device. Nothing to send.")
        return

    print(f"\nCollected a total of {len(all_measurements)} measurements. Preparing to send to OTel.")

    # Format all collected data into a single OTel message
    otel_message = _format_for_otel(
        metric_name="soil_moisture_percentage",
        unit="%",
        all_measurements=all_measurements
    )

    if otel_message is None:
        print("Failed to generate OTel message.")
        return

    # Send the combined message
    try:
        headers = {'Content-Type': 'application/json'}
        print(f"Sending combined OTel message to: {OTEL_SERVER_URL}")
        response = requests.post(OTEL_SERVER_URL, headers=headers, data=json.dumps(otel_message))
        response.raise_for_status()
        print(f"✅ Successfully sent OTel message. Status: {response.status_code}")
    except requests.exceptions.RequestException as e:
        print(f"❌ Error sending OTel message: {e}")


if __name__ == '__main__':
    collect_and_send_metrics()
