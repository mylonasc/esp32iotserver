import requests
import time
import pytest

BASE_URL = 'http://esp32-newname.local'
# How long to wait for the asynchronous humidity reading task to complete (in seconds)
TASK_TIMEOUT = 20
# How often to check the status of the task (in seconds)
POLL_INTERVAL = 1

# --- Test Suite for Humidity Endpoints ---


def print_raw_request(req):
    """
    Prints the raw HTTP request text for a given requests.PreparedRequest object.
    """
    # The request line (e.g., "POST /post HTTP/1.1")
    request_line = f"{req.method} {req.path_url} HTTP/1.1"

    # The headers, formatted as "Key: Value"
    headers = '\n'.join(f'{k}: {v}' for k, v in req.headers.items())

    # The request body
    body = req.body if req.body else ""

    # Decode body if it's in bytes for printing
    if isinstance(body, bytes):
        try:
            body = body.decode('utf-8')
        except UnicodeDecodeError:
            body = "[Binary data]"

    raw_request = f"{request_line}\n{headers}\n\n{body}"
    print("--- Raw Request ---")
    print(raw_request)
    print("-------------------")

def test_get_smooth_humi_settings():
    """
    Tests if we can successfully retrieve the default smooth humidity reading settings.
    Corresponds to: GET /api/smooth_humi/settings
    """
    try:
        response = requests.get(f"{BASE_URL}/api/smooth_humi/settings")
        
        # 1. Check for a successful response
        assert response.status_code == 200, f"Expected 200, got {response.status_code}. Response: {response.text}"
        
        # 2. Check if the response is valid JSON
        settings = response.json()
        assert isinstance(settings, dict), "Response is not a valid JSON object"
        
        # 3. Check for required keys and correct types
        assert "readings" in settings
        assert "interval_ms" in settings
        assert isinstance(settings["readings"], int)
        assert isinstance(settings["interval_ms"], int)
        
        print("\n[SUCCESS] GET /api/smooth_humi/settings returned valid settings.")

    except requests.exceptions.RequestException as e:
        pytest.fail(f"Failed to connect to the ESP32 at {BASE_URL}. Is it running and on the network? Error: {e}")


def test_update_and_verify_smooth_humi_settings():
    """
    Tests if we can update the settings and then verify that the update was successful.
    Corresponds to: POST /api/smooth_humi/settings followed by a GET.
    """
    # Store original settings to restore them later
    original_settings_response = requests.get(f"{BASE_URL}/api/smooth_humi/settings")
    assert original_settings_response.status_code == 200, f"Failed to get initial settings. Response: {original_settings_response.text}"
    original_settings = original_settings_response.json()

    new_settings = {
        "readings": 15,
        "interval_ms": 750
    }
    

    headers = {'Content-Type': 'application/json'}
    try:
        # --- Part 1: Update the settings (POST) ---
        response_post = requests.post(f"{BASE_URL}/api/smooth_humi/settings", json=new_settings, headers = headers)
        print_raw_request(response_post.request)

        assert response_post.status_code == 200, f"Expected 200, got {response_post.status_code}. Response: {response_post.text}"
        
        post_data = response_post.json()
        assert post_data.get("status") == "success"
        print(f"\n[SUCCESS] POST /api/smooth_humi/settings updated settings successfully.")

        # --- Part 2: Verify the new settings (GET) ---
        response_get = requests.get(f"{BASE_URL}/api/smooth_humi/settings")
        assert response_get.status_code == 200
        
        verified_settings = response_get.json()
        assert verified_settings["readings"] == new_settings["readings"]
        assert verified_settings["interval_ms"] == new_settings["interval_ms"]
        print(f"[SUCCESS] GET /api/smooth_humi/settings verified the new settings: {verified_settings}")

    finally:
        # --- Cleanup: Restore original settings ---
        requests.post(f"{BASE_URL}/api/smooth_humi/settings", json=original_settings, headers = headers)
        print(f"[INFO] Restored original settings: {original_settings}")


def test_trigger_smooth_humi_read_and_poll_for_result():
    """
    Tests the end-to-end flow of triggering a non-blocking humidity reading
    and polling the main /api endpoint until a result is available.
    Corresponds to: POST /api/smooth_humi/read followed by polling GET /api.
    """
    # First, find an enabled sensor to test with
    api_status_response = requests.get(f"{BASE_URL}/api")
    assert api_status_response.status_code == 200, f"Failed to get API status. Response: {api_status_response.text}"
    api_status = api_status_response.json()
    enabled_sensors = api_status.get("enabled_sensors", [])
    assert len(enabled_sensors) > 0, "No enabled sensors found on the device to test with."
    sensor_to_test = enabled_sensors[0]["label"]

    print(f"\n[INFO] Testing smooth read with sensor: '{sensor_to_test}'")

    # --- Part 1: Trigger the task ---
    trigger_payload = {"sensor_label": sensor_to_test}
    headers = {'Content-Type' : 'application/json'}
    response_trigger = requests.post(f"{BASE_URL}/api/smooth_humi/read", json=trigger_payload, headers = headers)
    
    # Expect '202 Accepted' to indicate the task has started
    assert response_trigger.status_code == 202, f"Expected 202, got {response_trigger.status_code}. Response: {response_trigger.text}"
    assert response_trigger.json().get("status") == "accepted"
    print("[SUCCESS] POST /api/smooth_humi/read accepted the task.")

    # --- Part 2: Poll for the result ---
    start_time = time.time()
    task_complete = False
    
    while time.time() - start_time < TASK_TIMEOUT:
        print(f"[INFO] Polling GET /api... (Elapsed: {time.time() - start_time:.1f}s)")
        response_status = requests.get(f"{BASE_URL}/api")
        assert response_status.status_code == 200
        status_data = response_status.json()

        if not status_data.get("smooth_humi_task_running"):
            print("[SUCCESS] Task is no longer running. Checking for results.")
            
            result = status_data.get("last_smooth_humi_result")
            assert result is not None, "Task finished but no result was found."
            
            # Verify the structure of the result
            assert result.get("sensor_label") == sensor_to_test
            assert "average_raw_value" in result
            assert "average_humidity_percent" in result
            print(result)
            # assert isinstance(result["average_raw_value"], float)
            #assert isinstance(result["average_humidity_percent"], float)
            
            print(f"[SUCCESS] Valid result received: {result}")
            task_complete = True
            break
        
        time.sleep(POLL_INTERVAL)

    assert task_complete, f"Task did not complete within the {TASK_TIMEOUT}s timeout."

def test_trigger_read_with_invalid_sensor():
    """
    Tests that the API correctly rejects a request with a non-existent sensor label.
    Corresponds to: POST /api/smooth_humi/read (Error Case)
    """
    trigger_payload = {"sensor_label": "This Sensor Does Not Exist"}
    response = requests.post(f"{BASE_URL}/api/smooth_humi/read", json=trigger_payload)
    
    # Expect '400 Bad Request' for an invalid label
    assert response.status_code == 400, f"Expected 400, got {response.status_code}. Response: {response.text}"
    print("\n[SUCCESS] API correctly returned 400 for an invalid sensor label.")

def test_trigger_read_while_task_is_running():
    """
    Tests that the API rejects a new task request if one is already in progress.
    Corresponds to: POST /api/smooth_humi/read (Error Case 503)
    """
    # Find an enabled sensor
    api_status_response = requests.get(f"{BASE_URL}/api")
    assert api_status_response.status_code == 200, f"Failed to get API status. Response: {api_status_response.text}"
    api_status = api_status_response.json()

    sensor_to_test = api_status["enabled_sensors"][0]["label"]
    payload = {"sensor_label": sensor_to_test}

    # Start a task (we don't need to wait for it to finish)
    response1 = requests.post(f"{BASE_URL}/api/smooth_humi/read", json=payload)
    if response1.status_code != 202:
        pytest.skip("Could not start the initial task, skipping this test.")

    print("\n[INFO] First task started successfully.")

    # Immediately try to start another one
    response2 = requests.post(f"{BASE_URL}/api/smooth_humi/read", json=payload)
    
    # Expect '503 Service Unavailable'
    assert response2.status_code == 503, f"Expected 503, got {response2.status_code}. Response: {response2.text}"
    print("[SUCCESS] API correctly returned 503 when trying to start a concurrent task.")

    # Wait for the first task to finish to not interfere with other tests
    time.sleep(5) # Give it a moment to complete


