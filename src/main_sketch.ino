#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ESP32Servo.h>
#include <WiFiProvisioner.h> 
#include <ArduinoJson.h> 

// --- NEW: DHT11 Library (Dhruba Saha) ---
// Ensure "DHT11" by Dhruba Saha is installed via Library Manager
#include <DHT11.h>

// pinout reference: 
// https://www.fambach.net/wp-content/uploads/D1_mini_ESP32_pinout.jpg

// --- Global Objects ---
Preferences preferences;
Servo myServo;
WebServer server(80);
WiFiProvisioner provisioner; 

// --- Configurable Application Settings (with default values) ---
// Pump Configuration
bool pumpA_enabled = true;
int pumpA_pin = 19;
bool pumpB_enabled = true;
int pumpB_pin = 20;
bool pumpC_enabled = true;
int pumpC_pin = 5;
int max_seconds_on = 10; 

// Servo Configuration
bool servo_enabled = true;
int servo_pin = 18;
int servo_initAngle = 90;   
int servo_finalAngle = 150;
int servo_millisMoveDelay = 5;

// Soil Humidity Sensor Configuration
bool soilSensA_enabled = true;
int soilSensA_pin = 35;
bool soilSensB_enabled = true;
int soilSensB_pin = 32;
String soilSensA_label = "Soil Sensor A";
String soilSensB_label = "Soil Sensor B";

// Soil Humidity Reading Task Configuration
bool soilTask_enabled = true;
long soilTask_interval = 300; 

// --- DHT11 Sensor Configuration ---
bool dht_enabled = false;
int dht_pin = 4; // Default pin
long dht_interval = 2000; // Min 2000ms recommended

// Smooth Reading Task Defaults
int smoothRead_defaults_readings = 5;
long smoothRead_defaults_interval = 1000;

// Network Configuration
String esp_hostname = "esp32"; 

// --- Global Variables for Application Logic (non-configurable) ---
int   _global_which_pump;
int   _global_time_seconds_on;
bool _global_sched_pump_task = false;
bool _global_sched_button = false;
bool _pump_task_lock = false;

unsigned long _previous_millis_read_soil = 0; 
unsigned long int millis_end_task; 

// Global Soil Humidity variables
int soil_a;
int soil_a_mapped;
int soil_b;
int soil_b_mapped;

// --- UPDATED: Global DHT Variables ---
DHT11* dhtSensor = nullptr; // Pointer to DHT11 object
int dht_temp_c = -999;      // Init to -999 to indicate "no read yet"
int dht_humidity = -999;    // Init to -999 to indicate "no read yet"
unsigned long _previous_millis_read_dht = 0;

const int led = 2; // 2 for D1 mini esp32

// --- Non-Blocking Servo Task State ---
enum ServoState { SERVO_IDLE, SERVO_MOVING_TO_FINAL, SERVO_MOVING_TO_INIT };
ServoState servoState = SERVO_IDLE;
int servoCurrentAngle = 90;
unsigned long servoLastMoveMillis = 0;

// --- Non-Blocking Smooth Humidity Reading Task State ---
bool _smoothHumi_task_running = false;
int _smoothHumi_readingsTaken = 0;
long _smoothHumi_rawSum = 0;
long _smoothHumi_mappedSum = 0;
int _smoothHumi_numReadings_task = 0;
long _smoothHumi_interval_task = 0;
int _smoothHumi_sensorPin_task = 0;
String _smoothHumi_sensorLabel_task = "";
unsigned long _smoothHumi_lastReadMillis = 0;

// --- Variables to store the final smooth reading result ---
float _smoothHumi_result_rawAvg = 0.0;
float _smoothHumi_result_mappedAvg = 0.0;
String _smoothHumi_result_label = "";
bool _smoothHumi_result_isNew = false; 

// --- WiFi Provisioning and Connection State Management ---
enum WiFiState {
  NOT_PROVISIONED,
  CONNECTING,
  CONNECTED,
  PROVISIONING
};
WiFiState currentWiFiState = NOT_PROVISIONED;
unsigned long lastConnectionAttemptMillis = 0;
const long CONNECTION_RETRY_INTERVAL = 10000; 
const int MAX_CONNECTION_ATTEMPTS = 5;      
int connectionAttemptsCount = 0;
String savedSSID = "";
String savedPassword = "";

// --- Function Prototypes ---
void loadConfig();
void saveConfig();
void startApplicationServices();
void handleRoot();
void handleSetMessage();
void handleConfig(); 
void handleNotFound();
void all_pumps_off();
void readSoilHumidity();
void handleServoCtrl();
void handleWateringPumps();
void handleDHT(); 
void handleServo();
void handlePump();
void readSoilHumidityTask();
void readDHTTask(); 
void connectToWiFi();
void startProvisioningMode();
void clearSavedCredentials();
void handleApi();
void handleSmoothHumiSettings();
void handleSmoothHumiRead();
void handleResetProvisioning();
void smoothHumiReadTask();
String generateJsonResponse();
String generateSmoothHumiDefaultsJson();
int getPinFromLabel(String label);
void initDHT(); 

// --- PROGMEM HTML Content for common elements ---
const char PROGMEM HTML_STYLE[] = R"rawliteral(
<style>
  body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; margin: 0; padding: 0; }
  .container { max-width: 600px; margin: 20px auto; padding: 20px; background-color: #ffffff; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
  h1, h2 { color: #000088; margin-bottom: 20px; }
  p { margin-bottom: 10px; }
  form { margin-top: 20px; }
  label { display: block; margin-bottom: 5px; font-weight: bold; }
  input[type='text'], input[type='number'], select { width: calc(100% - 22px); padding: 10px; margin-bottom: 10px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
  input[type='submit'], button { background-color: #4CAF50; color: white; padding: 10px 15px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin-right: 10px; margin-bottom: 10px; }
  input[type='submit']:hover, button:hover { background-color: #45a049; }
  .button-blue { background-color: #008CBA; }
  .button-blue:hover { background-color: #007B9E; }
  .checkbox-container { display: flex; align-items: center; margin-bottom: 10px; }
  .checkbox-container input { width: auto; margin-right: 10px; }
  .nav-links { margin-bottom: 20px; }
  .nav-links a { display: inline-block; padding: 10px 15px; background-color: #0056b3; color: white; text-decoration: none; border-radius: 4px; margin-right: 10px; }
  .nav-links a:hover { background-color: #004085; }
  .sensor-box { background-color: #eef; padding: 10px; border-radius: 4px; margin-bottom: 10px; border-left: 5px solid #0056b3; }
</style>
)rawliteral";

const char PROGMEM HTML_HEADER_COMMON[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 IoT Control</title>
)rawliteral";

const char PROGMEM HTML_FOOTER_COMMON[] = R"rawliteral(
</div>
</body>
</html>
)rawliteral";

// --- Helper to Generate Nav Links ---
String getNavLinks() {
    return String(F("<div class='nav-links'><a href='/'>Home</a><a href='/watering_pumps'>Pumps</a><a href='/servo_ctrl'>Servo</a><a href='/dht_sensor'>Air Sensor</a><a href='/config'>Config</a></div>"));
}

// --- Web Server Handlers ---

void handleRoot() {
  digitalWrite(led, 1);
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  preferences.begin("settings", true); 
  String storedMessage = preferences.getString("init-message", "no-value!");
  preferences.end();

  String htmlResponse = String(HTML_HEADER_COMMON);
  htmlResponse += String(HTML_STYLE);
  htmlResponse += String(F("</head><body><div class='container'>"));
  htmlResponse += getNavLinks();
  htmlResponse += String(F("<h1>Hello from ESP32!</h1>"));
  
  htmlResponse += String(F("<p>Uptime: "));
  if (hr < 10) htmlResponse += String(F("0"));
  htmlResponse += String(hr);
  htmlResponse += String(F(":"));
  if (min % 60 < 10) htmlResponse += String(F("0"));
  htmlResponse += String(min % 60);
  htmlResponse += String(F(":"));
  if (sec % 60 < 10) htmlResponse += String(F("0"));
  htmlResponse += String(sec % 60);
  htmlResponse += String(F("</p>"));

  // Overview of Soil Sensors
  htmlResponse += String(F("<div class='sensor-box'><b>Soil Status:</b><br>"));
  htmlResponse += soilSensA_label + String(F(": ")) + String(soil_a_mapped) + String(F("%<br>"));
  htmlResponse += soilSensB_label + String(F(": ")) + String(soil_b_mapped) + String(F("%</div>"));

  htmlResponse += String(F("<p>Message: ")) + storedMessage + String(F("</p>"));
  htmlResponse += String(F("  <form method='POST' action='/setMessage'>"));
  htmlResponse += String(F("    <label for='msg'>Enter new message:</label><br>"));
  htmlResponse += String(F("    <input type='text' id='msg' name='msg' size='40'><br><br>"));
  htmlResponse += String(F("    <input type='submit' value='Update Message'>"));
  htmlResponse += String(F("  </form>"));
  htmlResponse += String(HTML_FOOTER_COMMON);

  server.send(200, F("text/html"), htmlResponse);
  digitalWrite(led, 0);
}

void handleSetMessage() {
  if (!server.hasArg(F("msg"))) {
    server.send(400, F("text/plain"), F("Missing 'msg' field in the form."));
    return;
  }

  String newMessage = server.arg(F("msg"));

  preferences.begin("settings", false); 
  preferences.putString("init-message", newMessage);
  preferences.end();

  server.sendHeader(F("Location"), F("/"));
  server.send(302, F("text/plain"), F("Message updated. Redirecting..."));
}

void handleConfig() {
  if (server.method() == HTTP_POST) {
    String old_esp_hostname = esp_hostname; 
    int old_dht_pin = dht_pin;

    loadConfig(); 

    pumpA_enabled = server.hasArg(F("pumpA_enabled")) ? true : false;
    pumpB_enabled = server.hasArg(F("pumpB_enabled")) ? true : false;
    pumpC_enabled = server.hasArg(F("pumpC_enabled")) ? true : false;

    if (server.hasArg(F("pumpA_pin"))) pumpA_pin = server.arg(F("pumpA_pin")).toInt();
    if (server.hasArg(F("pumpB_pin"))) pumpB_pin = server.arg(F("pumpB_pin")).toInt();
    if (server.hasArg(F("pumpC_pin"))) pumpC_pin = server.arg(F("pumpC_pin")).toInt();
    if (server.hasArg(F("max_seconds_on"))) max_seconds_on = server.arg(F("max_seconds_on")).toInt();

    servo_enabled = server.hasArg(F("servo_enabled")) ? true : false;
    if (server.hasArg(F("servo_pin"))) servo_pin = server.arg(F("servo_pin")).toInt();
    if (server.hasArg(F("servo_initAngle"))) servo_initAngle = server.arg(F("servo_initAngle")).toInt();
    if (server.hasArg(F("servo_finalAngle"))) servo_finalAngle = server.arg(F("servo_finalAngle")).toInt();
    if (server.hasArg(F("servo_millisMoveDelay"))) servo_millisMoveDelay = server.arg(F("servo_millisMoveDelay")).toInt();

    // Soil Logic
    soilSensA_enabled = server.hasArg(F("soilSensA_enabled")) ? true : false;
    if (server.hasArg(F("soilSensA_pin"))) soilSensA_pin = server.arg(F("soilSensA_pin")).toInt();
    if (server.hasArg(F("soilSensA_label"))) soilSensA_label = server.arg(F("soilSensA_label"));
    soilSensB_enabled = server.hasArg(F("soilSensB_enabled")) ? true : false;
    if (server.hasArg(F("soilSensB_pin"))) soilSensB_pin = server.arg(F("soilSensB_pin")).toInt();
    if (server.hasArg(F("soilSensB_label"))) soilSensB_label = server.arg(F("soilSensB_label"));

    soilTask_enabled = server.hasArg(F("soilTask_enabled")) ? true : false;
    if (server.hasArg(F("soilTask_interval"))) soilTask_interval = server.arg(F("soilTask_interval")).toInt();

    // DHT Logic
    dht_enabled = server.hasArg(F("dht_enabled")) ? true : false;
    if (server.hasArg(F("dht_pin"))) dht_pin = server.arg(F("dht_pin")).toInt();
    if (server.hasArg(F("dht_interval"))) dht_interval = server.arg(F("dht_interval")).toInt();

    if (server.hasArg(F("esp_hostname"))) esp_hostname = server.arg(F("esp_hostname"));

    saveConfig(); 
    Serial.println(F("Configuration updated and saved."));

    pinMode(pumpA_pin, OUTPUT); digitalWrite(pumpA_pin, LOW);
    pinMode(pumpB_pin, OUTPUT); digitalWrite(pumpB_pin, LOW);
    pinMode(pumpC_pin, OUTPUT); digitalWrite(pumpC_pin, LOW);
    pinMode(soilSensA_pin, INPUT);
    pinMode(soilSensB_pin, INPUT);

    // Re-init DHT if pin changed
    if (dht_enabled && dht_pin != old_dht_pin) {
        initDHT();
    }

    if (esp_hostname != old_esp_hostname) {
      server.send(200, F("text/html"), F("Configuration saved. Hostname changed. Rebooting ESP32 in 3 seconds...<meta http-equiv='refresh' content='3;url=/'/>"));
      delay(3000);
      ESP.restart(); 
    } else {
      server.sendHeader(F("Location"), F("/config"));
      server.send(302, F("text/plain"), F("Configuration updated. Redirecting..."));
    }
    return;
  }

  String htmlResponse = String(HTML_HEADER_COMMON);
  htmlResponse += String(HTML_STYLE);
  htmlResponse += String(F("</head><body><div class='container'>"));
  htmlResponse += getNavLinks();
  htmlResponse += String(F("<h1>ESP32 Configuration</h1>"));

  htmlResponse += String(F("<form method='POST' action='/config'>"));

  htmlResponse += String(F("<h2>Network Settings</h2>"));
  htmlResponse += String(F("<label for='esp_hostname'>ESP Hostname (.local):</label><input type='text' name='esp_hostname' value='"));
  htmlResponse += esp_hostname;
  htmlResponse += String(F("'><br>"));

  htmlResponse += String(F("<h2>Pump Settings</h2>"));
  htmlResponse += String(F("<div class='checkbox-container'><input type='checkbox' name='pumpA_enabled' id='pumpA_enabled' "));
  htmlResponse += (pumpA_enabled ? String(F("checked")) : String(F("")));
  htmlResponse += String(F("><label for='pumpA_enabled'>Enable Pump A</label></div>"));
  htmlResponse += String(F("<label for='pumpA_pin'>Pump A Pin:</label><input type='number' name='pumpA_pin' value='"));
  htmlResponse += String(pumpA_pin);
  htmlResponse += String(F("'><br>"));

  htmlResponse += String(F("<div class='checkbox-container'><input type='checkbox' name='pumpB_enabled' id='pumpB_enabled' "));
  htmlResponse += (pumpB_enabled ? String(F("checked")) : String(F("")));
  htmlResponse += String(F("><label for='pumpB_enabled'>Enable Pump B</label></div>"));
  htmlResponse += String(F("<label for='pumpB_pin'>Pump B Pin:</label><input type='number' name='pumpB_pin' value='"));
  htmlResponse += String(pumpB_pin);
  htmlResponse += String(F("'><br>"));

  htmlResponse += String(F("<div class='checkbox-container'><input type='checkbox' name='pumpC_enabled' id='pumpC_enabled' "));
  htmlResponse += (pumpC_enabled ? String(F("checked")) : String(F("")));
  htmlResponse += String(F("><label for='pumpC_enabled'>Enable Pump C</label></div>"));
  htmlResponse += String(F("<label for='pumpC_pin'>Pump C Pin:</label><input type='number' name='pumpC_pin' value='"));
  htmlResponse += String(pumpC_pin);
  htmlResponse += String(F("'><br>"));

  htmlResponse += String(F("<label for='max_seconds_on'>Max Pump ON Time (seconds):</label><input type='number' name='max_seconds_on' min='1' max='60' value='"));
  htmlResponse += String(max_seconds_on);
  htmlResponse += String(F("'><br>"));

  htmlResponse += String(F("<h2>Servo Settings</h2>"));
  htmlResponse += String(F("<div class='checkbox-container'><input type='checkbox' name='servo_enabled' id='servo_enabled' "));
  htmlResponse += (servo_enabled ? String(F("checked")) : String(F("")));
  htmlResponse += String(F("><label for='servo_enabled'>Enable Servo</label></div>"));
  htmlResponse += String(F("<label for='servo_pin'>Servo Pin:</label><input type='number' name='servo_pin' value='"));
  htmlResponse += String(servo_pin);
  htmlResponse += String(F("'><br>"));
  htmlResponse += String(F("<label for='servo_initAngle'>Initial Angle (degrees):</label><input type='number' name='servo_initAngle' min='0' max='180' value='"));
  htmlResponse += String(servo_initAngle);
  htmlResponse += String(F("'><br>"));
  htmlResponse += String(F("<label for='servo_finalAngle'>Final Angle (degrees):</label><input type='number' name='servo_finalAngle' min='0' max='180' value='"));
  htmlResponse += String(servo_finalAngle);
  htmlResponse += String(F("'><br>"));
  htmlResponse += String(F("<label for='servo_millisMoveDelay'>Move Delay (ms per degree):</label><input type='number' name='servo_millisMoveDelay' min='1' value='"));
  htmlResponse += String(servo_millisMoveDelay);
  htmlResponse += String(F("'><br>"));

  htmlResponse += String(F("<h2>Soil Humidity Sensor Settings</h2>"));
  htmlResponse += String(F("<div class='checkbox-container'><input type='checkbox' name='soilSensA_enabled' id='soilSensA_enabled' "));
  htmlResponse += (soilSensA_enabled ? String(F("checked")) : String(F("")));
  htmlResponse += String(F("><label for='soilSensA_enabled'>Enable Soil Sensor A</label></div>"));
  htmlResponse += String(F("<label for='soilSensA_pin'>Sensor A Pin:</label><input type='number' name='soilSensA_pin' value='"));
  htmlResponse += String(soilSensA_pin);
  htmlResponse += String(F("'><br>"));
  htmlResponse += String(F("<label for='soilSensA_label'>Sensor A Label:</label><input type='text' name='soilSensA_label' value='"));
  htmlResponse += soilSensA_label;
  htmlResponse += String(F("'><br>"));

  htmlResponse += String(F("<div class='checkbox-container'><input type='checkbox' name='soilSensB_enabled' id='soilSensB_enabled' "));
  htmlResponse += (soilSensB_enabled ? String(F("checked")) : String(F("")));
  htmlResponse += String(F("><label for='soilSensB_enabled'>Enable Soil Sensor B</label></div>"));
  htmlResponse += String(F("<label for='soilSensB_pin'>Sensor B Pin:</label><input type='number' name='soilSensB_pin' value='"));
  htmlResponse += String(soilSensB_pin);
  htmlResponse += String(F("'><br>"));
  htmlResponse += String(F("<label for='soilSensB_label'>Sensor B Label:</label><input type='text' name='soilSensB_label' value='"));
  htmlResponse += soilSensB_label;
  htmlResponse += String(F("'><br>"));

  htmlResponse += String(F("<h3>Soil Reading Task</h3>"));
  htmlResponse += String(F("<div class='checkbox-container'><input type='checkbox' name='soilTask_enabled' id='soilTask_enabled' "));
  htmlResponse += (soilTask_enabled ? String(F("checked")) : String(F("")));
  htmlResponse += String(F("><label for='soilTask_enabled'>Enable Soil Reading Task</label></div>"));
  htmlResponse += String(F("<label for='soilTask_interval'>Reading Interval (ms):</label><input type='number' name='soilTask_interval' min='100' value='"));
  htmlResponse += String(soilTask_interval);
  htmlResponse += String(F("'><br>"));
  
  // DHT Configuration Section
  htmlResponse += String(F("<h2>Air Sensor Settings (DHT11)</h2>"));
  htmlResponse += String(F("<div class='checkbox-container'><input type='checkbox' name='dht_enabled' id='dht_enabled' "));
  htmlResponse += (dht_enabled ? String(F("checked")) : String(F("")));
  htmlResponse += String(F("><label for='dht_enabled'>Enable DHT11</label></div>"));
  
  htmlResponse += String(F("<label for='dht_pin'>DHT Pin:</label><input type='number' name='dht_pin' value='"));
  htmlResponse += String(dht_pin);
  htmlResponse += String(F("'><br>"));
  
  htmlResponse += String(F("<label for='dht_interval'>Read Interval (ms, min 2000):</label><input type='number' name='dht_interval' min='2000' value='"));
  htmlResponse += String(dht_interval);
  htmlResponse += String(F("'><br>"));

  htmlResponse += String(F("<input type='submit' value='Save Configuration'>"));
  htmlResponse += String(F("</form>"));

  htmlResponse += String(F("<hr><h2>Device Management</h2>"));
  htmlResponse += String(F("<form action='/reset_wifi' method='POST'>"));
  htmlResponse += String(F("<p>This will clear all saved WiFi settings and restart the device in provisioning mode, allowing you to connect to a new network.</p>"));
  htmlResponse += String(F("<button type='submit' style='background-color:#d9534f;'>Reset WiFi & Reboot</button>"));
  htmlResponse += String(F("</form>"));
  
  htmlResponse += String(HTML_FOOTER_COMMON);

  server.send(200, F("text/html"), htmlResponse);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = String(F("File Not Found\n\n"));
  message += String(F("URI: "));
  message += server.uri();
  message += String(F("\nMethod: "));
  message += (server.method() == HTTP_GET) ? String(F("GET")) : String(F("POST"));
  message += String(F("\nArguments: "));
  message += server.args();
  message += String(F("\n"));

  for (uint8_t i = 0; i < server.args(); i++) {
    message += String(F(" ")) + server.argName(i) + String(F(": ")) + server.arg(i) + String(F("\n"));
  }

  server.send(404, F("text/plain"), message);
  digitalWrite(led, 0);
}

void all_pumps_off(){
  digitalWrite(pumpA_pin, LOW);
  digitalWrite(pumpB_pin, LOW);
  digitalWrite(pumpC_pin, LOW);
  millis_end_task = 0;
  _pump_task_lock = false;
  _global_sched_pump_task = false;
}

void readSoilHumidity(){
  if (soilSensA_enabled) {
    soil_a = analogRead(soilSensA_pin);
    soil_a_mapped = map(soil_a, 2300, 4095, 100, 0); 
  } else {
    soil_a = 0;
    soil_a_mapped = 0;
  }

  if (soilSensB_enabled) {
    soil_b = analogRead(soilSensB_pin);
    soil_b_mapped = map(soil_b, 2300, 4095, 100, 0); 
  } else {
    soil_b = 0;
    soil_b_mapped = 0;
  }
}

// --- UPDATED: DHT Handling Page ---
void handleDHT() {
    String message = String(HTML_HEADER_COMMON);
    message += String(HTML_STYLE);
    message += String(F("</head><body><div class='container'>"));
    message += getNavLinks();
    message += String(F("<h1>Air Condition (DHT11)</h1>"));

    if (!dht_enabled) {
        message += String(F("<p style='color:red;'>DHT Sensor is disabled in configuration.</p>"));
    } else {
        message += String(F("<div class='sensor-box'>"));
        message += String(F("<h2>Readings</h2>"));
        
        // Check for sentinel value (-999) indicating no read yet or error
        if (dht_temp_c == -999 || dht_humidity == -999) {
            message += String(F("<p>Status: <b>Reading Error / Initializing...</b></p>"));
        } else {
            message += String(F("<p style='font-size:24px;'>Temperature: <b>")) + String(dht_temp_c) + String(F(" &deg;C</b></p>"));
            message += String(F("<p style='font-size:24px;'>Humidity: <b>")) + String(dht_humidity) + String(F(" %</b></p>"));
        }
        message += String(F("<p><small>Last read: ")) + String((millis() - _previous_millis_read_dht) / 1000) + String(F(" seconds ago.</small></p>"));
        message += String(F("</div>"));
    }

    message += String(F("<button onclick=\"location.reload();\">Refresh Readings</button>"));
    message += String(HTML_FOOTER_COMMON);
    server.send(200, F("text/html"), message);
}

void handleServoCtrl(){
  String message = String(HTML_HEADER_COMMON);
  message += String(HTML_STYLE);
  message += String(F("</head><body><div class='container'>"));
  message += getNavLinks();
  message += String(F("<h1>Servo Control</h1>"));

  message += String(F("<form action=\"/servo_ctrl\" method=\"get\">"));
  message += String(F("    <button type=\"submit\" name=\"servo\" value=\"trigger\" class='button-blue'>Trigger Servo Movement</button>"));
  message += String(F("</form>"));

  if (servoState != SERVO_IDLE) {
    message += String(F("<p>Servo is currently moving...</p>"));
  }

  message += String(F("<div>")) + soilSensA_label + String(F(": ")) + String(soil_a_mapped) + String(F("% ")) + soilSensB_label + String(F(": ")) + String(soil_b_mapped) + String(F("%</div>"));
  message += String(F("<div>Pump Task Remaining: ")) + String( ((float)(millis_end_task > millis() ? (millis_end_task - millis()) : 0) / 1000.0), 1) + String(F(" seconds</div>"));

  if(server.hasArg(F("servo"))){
    if (servoState == SERVO_IDLE) {
      _global_sched_button = true;
      message += String(F("<p>Servo movement scheduled!</p>"));
    } else {
      message += String(F("<p>Servo is already in motion!</p>"));
    }
  }

  message += String(HTML_FOOTER_COMMON);
  server.send(200, F("text/html"), message);
}

void handleWateringPumps(){
  String message = String(HTML_HEADER_COMMON);
  message += String(HTML_STYLE);
  message += String(F("</head><body><div class='container'>"));
  message += getNavLinks();
  message += String(F("<h1>Watering Pumps Control</h1>"));

  message += String(F("<form action=\"/watering_pumps\" method=\"get\">"));
  if (pumpA_enabled) message += String(F("    <button type=\"submit\" name=\"pump\" value=\"pump_a\">Pump A</button>"));
  if (pumpB_enabled) message += String(F("    <button type=\"submit\" name=\"pump\" value=\"pump_b\">Pump B</button>"));
  if (pumpC_enabled) message += String(F("    <button type=\"submit\" name=\"pump\" value=\"pump_c\">Pump C</button>"));
  message += String(F("    <button type=\"submit\" name=\"pump\" value=\"all_off\" class='button-blue'>All Pumps Off</button>"));
  message += String(F("    <div>Time (seconds, max "));
  message += String(max_seconds_on);
  message += String(F("): <input type=\"number\" name=\"time_seconds_on\" min=\"1\" max=\""));
  message += String(max_seconds_on);
  message += String(F("\" value=\"5\"></div>"));
  message += String(F("</form>"));

  message += String(F("<div>")) + soilSensA_label + String(F(": ")) + String(soil_a_mapped) + String(F("% ")) + soilSensB_label + String(F(": ")) + String(soil_b_mapped) + String(F("%</div>"));
  message += String(F("<div>Pump Task Remaining: ")) + String( ((float)(millis_end_task > millis() ? (millis_end_task - millis()) : 0) / 1000.0), 1) + String(F(" seconds</div>"));

  if(server.hasArg(F("pump"))){
    String pumpArg = server.arg(F("pump"));
    if(pumpArg.equals(F("all_off"))){
      Serial.println(String(F("Turning all pumps off.")));
      all_pumps_off();
      message += String(F("<p>All pumps turned off!</p>"));
    } else if (server.hasArg(F("time_seconds_on"))) {
      int time_seconds_on = server.arg(F("time_seconds_on")).toInt();
      if (time_seconds_on > 0 && time_seconds_on <= max_seconds_on) {
        int which_pump_pin = 0;
        bool pump_is_enabled = false;

        if(pumpArg.equals(F("pump_a")) && pumpA_enabled){ which_pump_pin = pumpA_pin; pump_is_enabled = true; }
        else if(pumpArg.equals(F("pump_b")) && pumpB_enabled){ which_pump_pin = pumpB_pin; pump_is_enabled = true; }
        else if(pumpArg.equals(F("pump_c")) && pumpC_enabled){ which_pump_pin = pumpC_pin; pump_is_enabled = true; }

        if (pump_is_enabled && !_global_sched_pump_task) {
          _global_which_pump = which_pump_pin;
          _global_time_seconds_on = time_seconds_on;
          _global_sched_pump_task = true;
          message += String(F("<p>Pump ")) + pumpArg + String(F(" scheduled for ")) + String(time_seconds_on) + String(F(" seconds.</p>"));
        } else if (_global_sched_pump_task) {
          message += String(F("<p>A pump task is already running. Please wait.</p>"));
        } else if (!pump_is_enabled) {
          message += String(F("<p>Pump ")) + pumpArg + String(F(" is disabled in configuration.</p>"));
        }
      } else {
        message += String(F("<p>Invalid time duration. Must be between 1 and ")) + String(max_seconds_on) + String(F(" seconds.</p>"));
      }
    }
  }

  message += String(HTML_FOOTER_COMMON);
  server.send(200, F("text/html"), message);
}

void handleApi() {
  if (server.method() == HTTP_GET) {
    server.send(200, "application/json", generateJsonResponse());
  } else if (server.method() == HTTP_POST) {
    if (!server.hasHeader("Content-Type") || server.header("Content-Type").indexOf("application/json") == -1) {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Please send JSON data\"}");
      return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON body\"}");
      return;
    }

    if (doc.containsKey("max_seconds_on")) {
      max_seconds_on = doc["max_seconds_on"].as<int>();
      saveConfig();
      server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration updated\"}");
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"'max_seconds_on' key not found\"}");
    }
  }
}

void handleSmoothHumiSettings() {
  if (server.method() == HTTP_GET) {
    server.send(200, "application/json", generateSmoothHumiDefaultsJson());
    return;
  }

  if (server.method() == HTTP_POST) {
    if (!server.hasHeader("Content-Type") || server.header("Content-Type").indexOf("application/json") == -1) {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid Content-Type. Expected application/json.\"}");
      return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON body\"}");
      return;
    }

    if (doc.containsKey("readings") && doc.containsKey("interval_ms")) {
      smoothRead_defaults_readings = doc["readings"].as<int>();
      smoothRead_defaults_interval = doc["interval_ms"].as<long>();
      saveConfig();
      server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Smooth reading defaults updated.\"}");
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"To set defaults, please provide both 'readings' and 'interval_ms' fields.\"}");
    }
    return;
  }

  server.send(405, "application/json", "{\"status\":\"error\",\"message\":\"Method Not Allowed\"}");
}

void handleSmoothHumiRead() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"status\":\"error\",\"message\":\"Method Not Allowed. Use POST.\"}");
    return;
  }
  if (!server.hasHeader("Content-Type") || server.header("Content-Type").indexOf("application/json") == -1) {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid Content-Type. Expected application/json.\"}");
    return;
  }
  if (_smoothHumi_task_running) {
    server.send(503, "application/json", "{\"status\":\"error\",\"message\":\"Task already in progress. Please wait.\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON body\"}");
    return;
  }

  if (!doc.containsKey("sensor_label")) {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"'sensor_label' is required.\"}");
    return;
  }

  String sensorLabel = doc["sensor_label"].as<String>();
  int sensorPin = getPinFromLabel(sensorLabel);
  if (sensorPin == 0) {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid or disabled sensor label provided.\"}");
    return;
  }
  
  int numReadings = doc.containsKey("num_readings") ? doc["num_readings"].as<int>() : smoothRead_defaults_readings;
  long interval = doc.containsKey("interval_ms") ? doc["interval_ms"].as<long>() : smoothRead_defaults_interval;

  _smoothHumi_sensorPin_task = sensorPin;
  _smoothHumi_sensorLabel_task = sensorLabel;
  _smoothHumi_numReadings_task = numReadings;
  _smoothHumi_interval_task = interval;
  _smoothHumi_readingsTaken = 0;
  _smoothHumi_rawSum = 0;
  _smoothHumi_mappedSum = 0;
  _smoothHumi_lastReadMillis = millis() - interval; 
  _smoothHumi_result_isNew = false; 
  _smoothHumi_task_running = true;

  Serial.print("Starting non-blocking smooth reading for "); Serial.print(sensorLabel); Serial.println("...");
  server.send(202, "application/json", "{\"status\":\"accepted\",\"message\":\"Smooth reading task started. Check the /api endpoint for results later.\"}");
}

int getPinFromLabel(String label) {
  if (soilSensA_enabled && label.equals(soilSensA_label)) return soilSensA_pin;
  if (soilSensB_enabled && label.equals(soilSensB_label)) return soilSensB_pin;
  return 0;
}

// --- UPDATED: Generate JSON Response ---
String generateJsonResponse() {
  JsonDocument doc;

  doc["uptime_seconds"] = millis() / 1000;
  doc["wifi_status"] = (WiFi.status() == WL_CONNECTED) ? "Connected" : "Not Connected";
  
  // Soil
  doc["soil_a_raw"] = soil_a;
  doc["soil_a_mapped"] = soil_a_mapped;
  doc["soil_a_label"] = soilSensA_label;
  doc["soil_b_raw"] = soil_b;
  doc["soil_b_mapped"] = soil_b_mapped;
  doc["soil_b_label"] = soilSensB_label;

  // DHT (Check if valid read exists)
  if (dht_enabled && dht_temp_c != -999) {
    doc["dht_temp_c"] = dht_temp_c;
    doc["dht_humidity_percent"] = dht_humidity;
  } else if (dht_enabled) {
    doc["dht_status"] = "initializing_or_error";
  }

  doc["pump_task_active"] = _global_sched_pump_task;
  doc["pump_task_remaining_seconds"] = ((float)(millis_end_task > millis() ? (millis_end_task - millis()) : 0) / 1000.0);

  JsonObject config = doc.createNestedObject("config");
  config["max_seconds_on"] = max_seconds_on;
  config["servo_enabled"] = servo_enabled;
  config["soilTask_enabled"] = soilTask_enabled;
  config["dht_enabled"] = dht_enabled;

  JsonArray enabledSensors = doc.createNestedArray("enabled_sensors");
  if (soilSensA_enabled) {
    JsonObject sensorA = enabledSensors.createNestedObject();
    sensorA["label"] = soilSensA_label;
    sensorA["pin"] = soilSensA_pin;
    sensorA["type"] = "soil";
  }
  if (soilSensB_enabled) {
    JsonObject sensorB = enabledSensors.createNestedObject();
    sensorB["label"] = soilSensB_label;
    sensorB["pin"] = soilSensB_pin;
    sensorB["type"] = "soil";
  }
  if (dht_enabled) {
    JsonObject sensorDHT = enabledSensors.createNestedObject();
    sensorDHT["label"] = "Air Sensor";
    sensorDHT["pin"] = dht_pin;
    sensorDHT["type"] = "dht11";
  }
  
  doc["smooth_humi_task_running"] = _smoothHumi_task_running;
  
  if (_smoothHumi_result_isNew) {
    JsonObject result = doc.createNestedObject("last_smooth_humi_result");
    result["sensor_label"] = _smoothHumi_result_label;
    result["average_raw_value"] = _smoothHumi_result_rawAvg;
    result["average_humidity_percent"] = _smoothHumi_result_mappedAvg;
  } else {
    doc["last_smooth_humi_result"] = nullptr; 
  }
  
  String jsonOutput;
  serializeJson(doc, jsonOutput);
  return jsonOutput;
}

String generateSmoothHumiDefaultsJson() {
  JsonDocument doc;
  doc["readings"] = smoothRead_defaults_readings;
  doc["interval_ms"] = smoothRead_defaults_interval;
  
  String jsonOutput;
  serializeJson(doc, jsonOutput);
  return jsonOutput;
}


// --- Setup and Loop Functions ---

void setup(void) {
  Serial.begin(115200);
  Serial.println(F("\nESP32 Booting (Non-Blocking Version)..."));

  loadConfig();
  
  // --- UPDATED: Init DHT ---
  if (dht_enabled) {
    initDHT();
  }

  const char* headersToCollect[] = {"Content-Type", "User-Agent", "Content-Length"};
  size_t headerCount = sizeof(headersToCollect) / sizeof(char*);
  server.collectHeaders(headersToCollect, headerCount);

  pinMode(led, OUTPUT); digitalWrite(led, LOW);
  pinMode(pumpA_pin, OUTPUT); digitalWrite(pumpA_pin, LOW);
  pinMode(pumpB_pin, OUTPUT); digitalWrite(pumpB_pin, LOW);
  pinMode(pumpC_pin, OUTPUT); digitalWrite(pumpC_pin, LOW);
  pinMode(soilSensA_pin, INPUT);
  pinMode(soilSensB_pin, INPUT);

  if (savedSSID.length() > 0) {
    Serial.print(F("Found saved credentials. Attempting to connect to ")); Serial.println(savedSSID);
    currentWiFiState = CONNECTING;
    connectToWiFi();
  } else {
    Serial.println(F("No saved WiFi credentials. Starting provisioning mode."));
    currentWiFiState = PROVISIONING;
    startProvisioningMode();
  }
}

void loop(void) {
  switch (currentWiFiState) {
    case CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println(F("Successfully connected to WiFi!"));
        currentWiFiState = CONNECTED;
        startApplicationServices();
      } else if (millis() - lastConnectionAttemptMillis >= CONNECTION_RETRY_INTERVAL) {
        connectionAttemptsCount++;
        Serial.print(F("Connection attempt ")); Serial.print(connectionAttemptsCount); Serial.print(F("/")); Serial.println(MAX_CONNECTION_ATTEMPTS);
        if (connectionAttemptsCount < MAX_CONNECTION_ATTEMPTS) {
          connectToWiFi();
        } else {
          Serial.println(F("Max connection retries reached. Clearing credentials and starting provisioning."));
          currentWiFiState = PROVISIONING;
          clearSavedCredentials();
          startProvisioningMode();
        }
      }
      break;

    case CONNECTED:
      server.handleClient();
      handlePump();
      readSoilHumidityTask(); 
      readDHTTask(); 
      handleServo();
      smoothHumiReadTask();
      delay(2);
      break;

    case PROVISIONING:
      break;

    case NOT_PROVISIONED:
      break;
  }
}

// --- Configuration Management Functions ---

void loadConfig() {
  preferences.begin("config", true);
  pumpA_enabled = preferences.getBool("pumpA_en", true);
  pumpA_pin = preferences.getInt("pumpA_pin", 19);
  pumpB_enabled = preferences.getBool("pumpB_en", true);
  pumpB_pin = preferences.getInt("pumpB_pin", 20);
  pumpC_enabled = preferences.getBool("pumpC_en", true);
  pumpC_pin = preferences.getInt("pumpC_pin", 5);
  max_seconds_on = preferences.getInt("max_sec_on", 10);
  
  servo_enabled = preferences.getBool("servo_en", true);
  servo_pin = preferences.getInt("servo_pin", 18);
  servo_initAngle = preferences.getInt("servo_init", 90);
  servo_finalAngle = preferences.getInt("servo_final", 150);
  servo_millisMoveDelay = preferences.getInt("servo_delay", 5);
  
  soilSensA_enabled = preferences.getBool("humiA_en", true);
  soilSensA_pin = preferences.getInt("humiA_pin", 35);
  soilSensA_label = preferences.getString("humiA_label", "Soil Sensor A");
  soilSensB_enabled = preferences.getBool("humiB_en", true);
  soilSensB_pin = preferences.getInt("humiB_pin", 32);
  soilSensB_label = preferences.getString("humiB_label", "Soil Sensor B");
  soilTask_enabled = preferences.getBool("humiTask_en", true);
  soilTask_interval = preferences.getLong("humiTask_int", 300);
  
  dht_enabled = preferences.getBool("dht_en", false);
  dht_pin = preferences.getInt("dht_pin", 4);
  dht_interval = preferences.getLong("dht_int", 2000);
  
  smoothRead_defaults_readings = preferences.getInt("smooth_readings", 5);
  smoothRead_defaults_interval = preferences.getLong("smooth_interval", 1000);
  esp_hostname = preferences.getString("hostname", "esp32");
  preferences.end();
  
  preferences.begin("settings", true);
  savedSSID = preferences.getString("ssid", "");
  savedPassword = preferences.getString("password", "");
  preferences.end();
  Serial.println(F("Configuration loaded."));
}

void saveConfig() {
  preferences.begin("config", false);
  preferences.putBool("pumpA_en", pumpA_enabled);
  preferences.putInt("pumpA_pin", pumpA_pin);
  preferences.putBool("pumpB_en", pumpB_enabled);
  preferences.putInt("pumpB_pin", pumpB_pin);
  preferences.putBool("pumpC_en", pumpC_enabled);
  preferences.putInt("pumpC_pin", pumpC_pin);
  preferences.putInt("max_sec_on", max_seconds_on);
  
  preferences.putBool("servo_en", servo_enabled);
  preferences.putInt("servo_pin", servo_pin);
  preferences.putInt("servo_init", servo_initAngle);
  preferences.putInt("servo_final", servo_finalAngle);
  preferences.putInt("servo_delay", servo_millisMoveDelay);
  
  preferences.putBool("humiA_en", soilSensA_enabled);
  preferences.putInt("humiA_pin", soilSensA_pin);
  preferences.putString("humiA_label", soilSensA_label);
  preferences.putBool("humiB_en", soilSensB_enabled);
  preferences.putInt("humiB_pin", soilSensB_pin);
  preferences.putString("humiB_label", soilSensB_label);
  preferences.putBool("humiTask_en", soilTask_enabled);
  preferences.putLong("humiTask_int", soilTask_interval);
  
  preferences.putBool("dht_en", dht_enabled);
  preferences.putInt("dht_pin", dht_pin);
  preferences.putLong("dht_int", dht_interval);

  preferences.putInt("smooth_readings", smoothRead_defaults_readings);
  preferences.putLong("smooth_interval", smoothRead_defaults_interval);
  preferences.putString("hostname", esp_hostname);
  preferences.end();
  Serial.println(F("Configuration saved."));
}

// --- WiFi Management Functions ---

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
  lastConnectionAttemptMillis = millis();
}

void startProvisioningMode() {
  provisioner.getConfig().SHOW_INPUT_FIELD = false;
  provisioner.getConfig().SHOW_RESET_FIELD = false;
  provisioner.onSuccess(
    [](const char *ssid, const char *password, const char *input) {
      Serial.printf("Provisioning successful! Connected to SSID: %s\n", ssid);
      preferences.begin("settings", false);
      preferences.putString("ssid", ssid);
      if (password) { preferences.putString("password", password); }
      else { preferences.remove("password"); }
      preferences.end();
      savedSSID = ssid;
      savedPassword = password ? String(password) : F("");
      currentWiFiState = CONNECTING;
      connectionAttemptsCount = 0;
      lastConnectionAttemptMillis = millis();
    });
  provisioner.startProvisioning();
  Serial.println(F("WiFi Provisioning started. Connect to AP 'ESP_PROV' and navigate to 192.168.4.1"));
}

void clearSavedCredentials() {
  preferences.begin("settings", false);
  preferences.remove("ssid");
  preferences.remove("password");
  preferences.end();
  savedSSID = F("");
  savedPassword = F("");
  Serial.println(F("Cleared saved WiFi credentials from flash."));
}

void handleResetProvisioning() {
  server.send(200, F("text/plain"), F("WiFi credentials cleared. The device will now reboot into provisioning mode."));
  delay(1000);
  clearSavedCredentials();
  ESP.restart();
}

void startApplicationServices() {
  Serial.print(F("IP address: ")); Serial.println(WiFi.localIP());
  if (MDNS.begin(esp_hostname.c_str())) {
    Serial.print(F("MDNS responder started at http://")); Serial.print(esp_hostname); Serial.println(F(".local"));
  } else { Serial.println(F("Error starting MDNS responder!")); }

  server.on(F("/"), handleRoot);
  server.on(F("/watering_pumps"), handleWateringPumps);
  server.on(F("/servo_ctrl"), handleServoCtrl);
  server.on(F("/dht_sensor"), handleDHT);
  server.on(F("/config"), HTTP_GET, handleConfig);
  server.on(F("/config"), HTTP_POST, handleConfig);
  server.on(F("/setMessage"), HTTP_POST, handleSetMessage);
  server.on(F("/api"), handleApi);
  server.on(F("/api/smooth_humi/settings"), HTTP_GET, handleSmoothHumiSettings);
  server.on(F("/api/smooth_humi/settings"), HTTP_POST, handleSmoothHumiSettings);
  server.on(F("/api/smooth_humi/read"), HTTP_POST, handleSmoothHumiRead);
  server.on(F("/reset_wifi"), HTTP_POST, handleResetProvisioning);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println(F("HTTP server started"));
}

// --- Application Task Functions ---

void handleServo(){
  if (!servo_enabled) {
    if (servoState != SERVO_IDLE) { 
      myServo.detach();
      servoState = SERVO_IDLE;
    }
    return;
  }

  if (_global_sched_button && servoState == SERVO_IDLE) {
    _global_sched_button = false; 
    myServo.attach(servo_pin);
    servoCurrentAngle = servo_initAngle;
    myServo.write(servoCurrentAngle);
    servoState = SERVO_MOVING_TO_FINAL;
    servoLastMoveMillis = millis();
    Serial.println(F("Starting non-blocking servo movement..."));
  }

  unsigned long currentMillis = millis();
  if (currentMillis - servoLastMoveMillis < servo_millisMoveDelay) {
    return; 
  }
  servoLastMoveMillis = currentMillis;

  switch (servoState) {
    case SERVO_MOVING_TO_FINAL:
      if (servoCurrentAngle < servo_finalAngle) {
        servoCurrentAngle++;
        myServo.write(servoCurrentAngle);
      } else {
        servoState = SERVO_MOVING_TO_INIT; 
      }
      break;
    
    case SERVO_MOVING_TO_INIT:
      if (servoCurrentAngle > servo_initAngle) {
        servoCurrentAngle--;
        myServo.write(servoCurrentAngle);
      } else {
        myServo.detach(); 
        servoState = SERVO_IDLE;
        Serial.println(F("Non-blocking servo movement complete."));
      }
      break;

    case SERVO_IDLE:
    default:
      break;
  }
}

void handlePump(){
  if(_global_sched_pump_task){
    if(!_pump_task_lock){
      _pump_task_lock = true;
      millis_end_task = millis() + (unsigned long)_global_time_seconds_on * 1000;
      digitalWrite(_global_which_pump, HIGH);
      Serial.print(String(F("Pump "))); Serial.print(_global_which_pump); Serial.print(String(F(" ON for "))); Serial.print(_global_time_seconds_on); Serial.println(String(F(" seconds.")));
    }
  }
  if(_pump_task_lock && _global_sched_pump_task){
    if (millis() > millis_end_task){
      Serial.println(String(F("Terminating pump task.")));
      digitalWrite(_global_which_pump, LOW);
      _pump_task_lock = false;
      millis_end_task = 0;
      _global_which_pump = 0;
      _global_sched_pump_task = false;
    }
  }
}

void readSoilHumidityTask(){
  if (soilTask_enabled) {
    unsigned long _curr_millis = millis();
    if( ( _curr_millis - _previous_millis_read_soil )>= soilTask_interval){
      readSoilHumidity();
      _previous_millis_read_soil = _curr_millis;
    }
  }
}

// --- UPDATED: DHT Task using Dhruba Saha Library ---
void readDHTTask() {
    if (dht_enabled && dhtSensor != nullptr) {
        unsigned long _curr_millis = millis();
        if( ( _curr_millis - _previous_millis_read_dht ) >= dht_interval){
            _previous_millis_read_dht = _curr_millis;
            
            // Pass variables by reference to the library
            int t, h;
            int result = dhtSensor->readTemperatureHumidity(t, h);

            if (result == 0) {
                // Success
                dht_temp_c = t;
                dht_humidity = h;
                // Uncomment to debug:
                // Serial.print("DHT Read: "); Serial.print(t); Serial.print("C "); Serial.print(h); Serial.println("%");
            } else {
                // Error - Print the error string from library
                Serial.print(F("DHT Error: "));
                Serial.println(DHT11::getErrorString(result));
            }
        }
    }
}

// --- UPDATED: Helper to Init DHT with Dhruba Saha Library ---
void initDHT() {
    // Delete previous instance if it exists (handling pin changes)
    if (dhtSensor != nullptr) {
        delete dhtSensor;
    }
    Serial.print(F("Initializing DHT11 on pin ")); Serial.println(dht_pin);
    
    // Create new instance with the configured pin
    dhtSensor = new DHT11(dht_pin);
    // Note: This library does not use .begin()
}

void smoothHumiReadTask() {
  if (!_smoothHumi_task_running) {
    return;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - _smoothHumi_lastReadMillis >= _smoothHumi_interval_task) {
    _smoothHumi_lastReadMillis = currentMillis;

    int raw = analogRead(_smoothHumi_sensorPin_task);
    int mapped = map(raw, 2300, 4095, 100, 0);
    _smoothHumi_rawSum += raw;
    _smoothHumi_mappedSum += mapped;
    _smoothHumi_readingsTaken++;
    
    Serial.printf("Smooth read %d/%d for %s\n", _smoothHumi_readingsTaken, _smoothHumi_numReadings_task, _smoothHumi_sensorLabel_task.c_str());

    if (_smoothHumi_readingsTaken >= _smoothHumi_numReadings_task) {
      _smoothHumi_result_rawAvg = (float)_smoothHumi_rawSum / _smoothHumi_numReadings_task;
      _smoothHumi_result_mappedAvg = (float)_smoothHumi_mappedSum / _smoothHumi_numReadings_task;
      _smoothHumi_result_label = _smoothHumi_sensorLabel_task;
      _smoothHumi_result_isNew = true; 

      _smoothHumi_task_running = false;
      Serial.println("Smooth humidity reading task finished.");
    }
  }
}
