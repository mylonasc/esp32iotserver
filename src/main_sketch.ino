#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ESP32Servo.h>
#include <WiFiProvisioner.h> 

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

// Humidity Sensor Configuration
bool humiSensA_enabled = true;
int humiSensA_pin = 35;
bool humiSensB_enabled = true;
int humiSensB_pin = 32;
String humiSensA_label = "Sensor A";
String humiSensB_label = "Sensor B";

// Humidity Reading Task Configuration
bool humiTask_enabled = true;
long humiTask_interval = 300; 

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
bool _global_smooth_humi_task_running = false; // New variable for smooth reading task

unsigned long _previous_millis_read_sensors = 0; 
unsigned long int millis_end_task; 

// Global humidity variables
int humi_a;
int humi_a_mapped;
int humi_b;
int humi_b_mapped;

const int led = 2; // 2 for D1 mini esp32

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
void readHumidity();
void handleServoCtrl();
void handleWateringPumps();
void handleServo();
void handlePump();
void readHumidityTask();
void connectToWiFi();
void startProvisioningMode();
void clearSavedCredentials();
void handleApi();
void handleSmoothHumi();
String generateJsonResponse();
String generateSmoothHumiDefaultsJson();
String getEnabledSensorsJson();
int getPinFromLabel(String label);

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

// --- Web Server Handlers ---

/**
 * @brief Handles the root URL ("/") to display system info and a message form.
 * Uses PROGMEM for static HTML and String for dynamic content.
 * Removed automatic refresh.
 */
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
  htmlResponse += String(F("<div class='nav-links'><a href='/'>Home</a><a href='/watering_pumps'>Pumps</a><a href='/servo_ctrl'>Servo</a><a href='/config'>Config</a></div>"));
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

/**
 * @brief Handles POST requests to "/setMessage" to update a stored message.
 * Expects a form field "msg" containing the new message.
 */
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

/**
 * @brief Handles configuration settings for pumps, servos, and humidity sensors.
 * Allows enabling/disabling and setting parameters.
 */
void handleConfig() {
  if (server.method() == HTTP_POST) {
    String old_esp_hostname = esp_hostname; 

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

    humiSensA_enabled = server.hasArg(F("humiSensA_enabled")) ? true : false;
    if (server.hasArg(F("humiSensA_pin"))) humiSensA_pin = server.arg(F("humiSensA_pin")).toInt();
    if (server.hasArg(F("humiSensA_label"))) humiSensA_label = server.arg(F("humiSensA_label"));
    humiSensB_enabled = server.hasArg(F("humiSensB_enabled")) ? true : false;
    if (server.hasArg(F("humiSensB_pin"))) humiSensB_pin = server.arg(F("humiSensB_pin")).toInt();
    if (server.hasArg(F("humiSensB_label"))) humiSensB_label = server.arg(F("humiSensB_label"));

    humiTask_enabled = server.hasArg(F("humiTask_enabled")) ? true : false;
    if (server.hasArg(F("humiTask_interval"))) humiTask_interval = server.arg(F("humiTask_interval")).toInt();

    if (server.hasArg(F("esp_hostname"))) esp_hostname = server.arg(F("esp_hostname"));

    saveConfig(); 
    Serial.println(F("Configuration updated and saved."));

    pinMode(pumpA_pin, OUTPUT); digitalWrite(pumpA_pin, LOW);
    pinMode(pumpB_pin, OUTPUT); digitalWrite(pumpB_pin, LOW);
    pinMode(pumpC_pin, OUTPUT); digitalWrite(pumpC_pin, LOW);
    pinMode(humiSensA_pin, INPUT);
    pinMode(humiSensB_pin, INPUT);

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
  htmlResponse += String(F("<div class='nav-links'><a href='/'>Home</a><a href='/watering_pumps'>Pumps</a><a href='/servo_ctrl'>Servo</a><a href='/config'>Config</a></div>"));
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

  htmlResponse += String(F("<h2>Humidity Sensor Settings</h2>"));
  htmlResponse += String(F("<div class='checkbox-container'><input type='checkbox' name='humiSensA_enabled' id='humiSensA_enabled' "));
  htmlResponse += (humiSensA_enabled ? String(F("checked")) : String(F("")));
  htmlResponse += String(F("><label for='humiSensA_enabled'>Enable Humidity Sensor A</label></div>"));
  htmlResponse += String(F("<label for='humiSensA_pin'>Sensor A Pin:</label><input type='number' name='humiSensA_pin' value='"));
  htmlResponse += String(humiSensA_pin);
  htmlResponse += String(F("'><br>"));
  htmlResponse += String(F("<label for='humiSensA_label'>Sensor A Label:</label><input type='text' name='humiSensA_label' value='"));
  htmlResponse += humiSensA_label;
  htmlResponse += String(F("'><br>"));

  htmlResponse += String(F("<div class='checkbox-container'><input type='checkbox' name='humiSensB_enabled' id='humiSensB_enabled' "));
  htmlResponse += (humiSensB_enabled ? String(F("checked")) : String(F("")));
  htmlResponse += String(F("><label for='humiSensB_enabled'>Enable Humidity Sensor B</label></div>"));
  htmlResponse += String(F("<label for='humiSensB_pin'>Sensor B Pin:</label><input type='number' name='humiSensB_pin' value='"));
  htmlResponse += String(humiSensB_pin);
  htmlResponse += String(F("'><br>"));
  htmlResponse += String(F("<label for='humiSensB_label'>Sensor B Label:</label><input type='text' name='humiSensB_label' value='"));
  htmlResponse += humiSensB_label;
  htmlResponse += String(F("'><br>"));

  htmlResponse += String(F("<h2>Humidity Reading Task</h2>"));
  htmlResponse += String(F("<div class='checkbox-container'><input type='checkbox' name='humiTask_enabled' id='humiTask_enabled' "));
  htmlResponse += (humiTask_enabled ? String(F("checked")) : String(F("")));
  htmlResponse += String(F("><label for='humiTask_enabled'>Enable Humidity Reading Task</label></div>"));
  htmlResponse += String(F("<label for='humiTask_interval'>Reading Interval (ms):</label><input type='number' name='humiTask_interval' min='100' value='"));
  htmlResponse += String(humiTask_interval);
  htmlResponse += String(F("'><br>"));
  
  htmlResponse += String(F("<input type='submit' value='Save Configuration'>"));
  htmlResponse += String(F("</form>"));
  htmlResponse += String(HTML_FOOTER_COMMON);

  server.send(200, F("text/html"), htmlResponse);
}


/**
 * @brief Handles requests for unknown URLs.
 */
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

/**
 * @brief Turns off all pumps and resets pump task variables.
 */
void all_pumps_off(){
  digitalWrite(pumpA_pin, LOW);
  digitalWrite(pumpB_pin, LOW);
  digitalWrite(pumpC_pin, LOW);
  millis_end_task = 0;
  _pump_task_lock = false;
  _global_sched_pump_task = false;
}

/**
 * @brief Reads humidity sensor values and maps them to a 0-100% range.
 * Respects sensor enable flags and configured pins.
 */
void readHumidity(){
  if (humiSensA_enabled) {
    humi_a = analogRead(humiSensA_pin);
    humi_a_mapped = map(humi_a, 2300, 4095, 100, 0); // Adjust calibration as needed
    Serial.print(humiSensA_label); Serial.print(String(F(" Raw: "))); Serial.print(humi_a); Serial.print(String(F(" Mapped: "))); Serial.println(humi_a_mapped);
  } else {
    humi_a = 0; // Or some indicator that it's disabled
    humi_a_mapped = 0;
  }

  if (humiSensB_enabled) {
    humi_b = analogRead(humiSensB_pin);
    humi_b_mapped = map(humi_b, 2300, 4095, 100, 0); // Adjust calibration as needed
    Serial.print(humiSensB_label); Serial.print(String(F(" Raw: "))); Serial.print(humi_b); Serial.print(String(F(" Mapped: "))); Serial.println(humi_b_mapped);
  } else {
    humi_b = 0;
    humi_b_mapped = 0;
  }
}

/**
 * @brief Handles servo control requests via a web interface.
 * Allows setting servo angles and movement delay.
 * Uses String concatenation for dynamic HTML generation.
 */
void handleServoCtrl(){
  String message = String(HTML_HEADER_COMMON);
  message += String(HTML_STYLE);
  message += String(F("</head><body><div class='container'>"));
  message += String(F("<div class='nav-links'><a href='/'>Home</a><a href='/watering_pumps'>Pumps</a><a href='/servo_ctrl'>Servo</a><a href='/config'>Config</a></div>"));
  message += String(F("<h1>Servo Control</h1>"));

  message += String(F("<form action=\"/servo_ctrl\" method=\"get\">")); // Changed to GET for simplicity in this example
  message += String(F("    <button type=\"submit\" name=\"servo\" value=\"trigger\" class='button-blue'>Trigger Servo Movement</button>"));
  message += String(F("    <div>Millis Move Delay: <input type=\"text\" name=\"millis_move_delay\" value=\""));
  message += String(servo_millisMoveDelay);
  message += String(F("\"></div>"));
  message += String(F("    <div>Initial Angle: <input type=\"text\" name=\"init_angle\" value=\""));
  message += String(servo_initAngle);
  message += String(F("\"></div>"));
  message += String(F("    <div>Final Angle: <input type=\"text\" name=\"final_angle\" value=\""));
  message += String(servo_finalAngle);
  message += String(F("\"></div>"));
  message += String(F("</form>"));

  message += String(F("<div>")) + humiSensA_label + String(F(": ")) + String(humi_a_mapped) + String(F("% ")) + humiSensB_label + String(F(": ")) + String(humi_b_mapped) + String(F("%</div>"));
  message += String(F("<div>Pump Task Remaining: ")) + String( ((float)(millis_end_task > millis() ? (millis_end_task - millis()) : 0) / 1000.0), 1) + String(F(" seconds</div>"));

  // Process servo arguments if present
  if(server.hasArg(F("servo"))){
    if (server.hasArg(F("millis_move_delay"))){
      servo_millisMoveDelay = server.arg(F("millis_move_delay")).toInt();
      Serial.print(String(F("Set servo_millisMoveDelay: "))); Serial.println(servo_millisMoveDelay);
    }
    if (server.hasArg(F("init_angle"))){
      servo_initAngle = server.arg(F("init_angle")).toInt();
      Serial.print(String(F("Set servo_initAngle: "))); Serial.println(servo_initAngle);
    }
    if(server.hasArg(F("final_angle"))){
      servo_finalAngle = server.arg(F("final_angle")).toInt();
      Serial.print(String(F("Set servo_finalAngle: "))); Serial.println(servo_finalAngle);
    }
    _global_sched_button = true; // Schedule servo movement
    message += String(F("<p>Servo movement scheduled!</p>"));
  }

  message += String(HTML_FOOTER_COMMON);
  server.send(200, F("text/html"), message);
}

/**
 * @brief Handles watering pump control requests via a web interface.
 * Allows turning pumps on for a specified duration.
 * Uses String concatenation for dynamic HTML generation.
 */
void handleWateringPumps(){
  String message = String(HTML_HEADER_COMMON);
  message += String(HTML_STYLE);
  message += String(F("</head><body><div class='container'>"));
  message += String(F("<div class='nav-links'><a href='/'>Home</a><a href='/watering_pumps'>Pumps</a><a href='/servo_ctrl'>Servo</a><a href='/config'>Config</a></div>"));
  message += String(F("<h1>Watering Pumps Control</h1>"));

  message += String(F("<form action=\"/watering_pumps\" method=\"get\">")); // Changed to GET for simplicity
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

  message += String(F("<div>")) + humiSensA_label + String(F(": ")) + String(humi_a_mapped) + String(F("% ")) + humiSensB_label + String(F(": ")) + String(humi_b_mapped) + String(F("%</div>"));
  message += String(F("<div>Pump Task Remaining: ")) + String( ((float)(millis_end_task > millis() ? (millis_end_task - millis()) : 0) / 1000.0), 1) + String(F(" seconds</div>"));

  // Process pump arguments
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

        if(pumpArg.equals(F("pump_a")) && pumpA_enabled){
          which_pump_pin = pumpA_pin;
          pump_is_enabled = true;
        } else if(pumpArg.equals(F("pump_b")) && pumpB_enabled){
          which_pump_pin = pumpB_pin;
          pump_is_enabled = true;
        } else if(pumpArg.equals(F("pump_c")) && pumpC_enabled){
          which_pump_pin = pumpC_pin;
          pump_is_enabled = true;
        }

        if (pump_is_enabled && !_global_sched_pump_task) { // Only schedule if enabled and no pump task is active
          _global_which_pump = which_pump_pin;
          _global_time_seconds_on = time_seconds_on;
          _global_sched_pump_task = true;
          Serial.print(String(F("Scheduled pump "))); Serial.print(pumpArg); Serial.print(String(F(" for "))); Serial.print(time_seconds_on); Serial.println(String(F(" seconds.")));
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

/**
 * @brief Handles the /api endpoint for RESTful access to device data and controls.
 */
void handleApi() {
  if (server.method() == HTTP_GET) {
    server.send(200, "application/json", generateJsonResponse());
  } else if (server.method() == HTTP_POST) {
    // Check for JSON content type
    if (server.hasHeader("Content-Type") && server.header("Content-Type").indexOf("application/json") != -1) {
      String json = server.arg("plain");
      // You would use a JSON parsing library here for a more robust solution.
      // For this example, we'll use simple string parsing.
      if (json.indexOf("\"max_seconds_on\"") != -1) {
        int start = json.indexOf("\"max_seconds_on\":") + 18;
        int end = json.indexOf("}", start);
        if (end == -1) end = json.indexOf(",", start);
        if (end != -1) {
          String valueStr = json.substring(start, end);
          valueStr.trim(); // Trim the string after creating it
          max_seconds_on = valueStr.toInt();
        }
      }
      // Add similar parsing for other configurable variables here.
      
      saveConfig();
      server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration updated\"}");
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Please send JSON data\"}");
    }
  }
}

/**
 * @brief Handles the /api/smooth_humi endpoint to get/set defaults or perform a series of readings.
 */
void handleSmoothHumi() {
  // Handle GET request to return current defaults
  if (server.method() == HTTP_GET) {
    server.send(200, "application/json", generateSmoothHumiDefaultsJson());
    return;
  }
  
  // Handle POST request
  if (server.method() == HTTP_POST) {
    if (!server.hasHeader("Content-Type") || server.header("Content-Type").indexOf("application/json") == -1) {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid Content-Type. Expected application/json.\"}");
      return;
    }

    String json = server.arg("plain");
    
    // Check for "trigger" to start the task
    if (json.indexOf("\"trigger\":true") != -1) {
      if (_global_smooth_humi_task_running) {
        server.send(503, "application/json", "{\"status\":\"error\",\"message\":\"Task already in progress. Please wait.\"}");
        return;
      }

      String sensorLabel = "";
      int numReadings = smoothRead_defaults_readings;
      long interval = smoothRead_defaults_interval;

      // Extract sensor label from JSON
      int labelStart = json.indexOf("\"sensor_label\":\"") + 16;
      if (labelStart != 15) { // 15 is the index if not found
        int labelEnd = json.indexOf("\"", labelStart);
        sensorLabel = json.substring(labelStart, labelEnd);
      } else {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"'sensor_label' is required to trigger a reading.\"}");
        return;
      }

      int sensorPin = getPinFromLabel(sensorLabel);
      if (sensorPin == 0) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid or disabled sensor label provided.\"}");
        return;
      }
      
      // Extract optional reading parameters from JSON
      if (json.indexOf("\"num_readings\"") != -1) {
        numReadings = json.substring(json.indexOf("\"num_readings\":") + 15, json.indexOf(",", json.indexOf("\"num_readings\":"))).toInt();
      }
      if (json.indexOf("\"interval_ms\"") != -1) {
        interval = json.substring(json.indexOf("\"interval_ms\":") + 14, json.indexOf(",", json.indexOf("\"interval_ms\":"))).toInt();
      }

      if (numReadings <= 0) numReadings = smoothRead_defaults_readings;
      if (interval <= 0) interval = smoothRead_defaults_interval;
      
      _global_smooth_humi_task_running = true;
      Serial.print("Starting smooth reading for "); Serial.print(sensorLabel); Serial.println("...");

      long rawSum = 0;
      long mappedSum = 0;
      
      for (int i = 0; i < numReadings; i++) {
        int raw = analogRead(sensorPin);
        int mapped = map(raw, 2300, 4095, 100, 0);
        rawSum += raw;
        mappedSum += mapped;
        delay(interval);
      }

      _global_smooth_humi_task_running = false;

      float rawAvg = (float)rawSum / numReadings;
      float mappedAvg = (float)mappedSum / numReadings;

      String responseJson = "{";
      responseJson += "\"status\":\"success\",";
      responseJson += "\"sensor_label\":\"" + sensorLabel + "\",";
      responseJson += "\"num_readings\":" + String(numReadings) + ",";
      responseJson += "\"interval_ms\":" + String(interval) + ",";
      responseJson += "\"average_raw_value\":" + String(rawAvg, 2) + ",";
      responseJson += "\"average_humidity_percent\":" + String(mappedAvg, 2);
      responseJson += "}";

      server.send(200, "application/json", responseJson);
      return;
    } 
    // Handle POST request to set defaults
    else {
      if (json.indexOf("\"readings\"") != -1 && json.indexOf("\"interval_ms\"") != -1) {
        int readingsStart = json.indexOf("\"readings\":") + 11;
        int readingsEnd = json.indexOf(",", readingsStart);
        smoothRead_defaults_readings = json.substring(readingsStart, readingsEnd).toInt();

        int intervalStart = json.indexOf("\"interval_ms\":") + 14;
        int intervalEnd = json.indexOf("}", intervalStart);
        smoothRead_defaults_interval = json.substring(intervalStart, intervalEnd).toInt();

        saveConfig();
        server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Smooth reading defaults updated.\"}");
      } else {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"To set defaults, please provide both 'readings' and 'interval_ms' fields.\"}");
      }
    }
  }
}

/**
 * @brief Helper function to get the sensor pin from its label.
 * @param label The label of the sensor.
 * @return The GPIO pin number, or 0 if not found or disabled.
 */
int getPinFromLabel(String label) {
  if (humiSensA_enabled && label.equals(humiSensA_label)) {
    return humiSensA_pin;
  }
  if (humiSensB_enabled && label.equals(humiSensB_label)) {
    return humiSensB_pin;
  }
  return 0;
}

/**
 * @brief Generates a JSON response with the current system state.
 */
String generateJsonResponse() {
  String json = "{";
  json += "\"uptime_seconds\":" + String(millis() / 1000) + ",";
  json += "\"wifi_status\":\"" + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Not Connected") + "\",";
  json += "\"humi_a_raw\":" + String(humi_a) + ",";
  json += "\"humi_a_mapped\":" + String(humi_a_mapped) + ",";
  json += "\"humi_a_label\":\"" + humiSensA_label + "\",";
  json += "\"humi_b_raw\":" + String(humi_b) + ",";
  json += "\"humi_b_mapped\":" + String(humi_b_mapped) + ",";
  json += "\"humi_b_label\":\"" + humiSensB_label + "\",";
  json += "\"pump_task_active\":" + String(_global_sched_pump_task ? "true" : "false") + ",";
  json += "\"pump_task_remaining_seconds\":" + String( ((float)(millis_end_task > millis() ? (millis_end_task - millis()) : 0) / 1000.0), 1) + ",";
  json += "\"config\":{";
  json += "\"max_seconds_on\":" + String(max_seconds_on) + ",";
  json += "\"servo_enabled\":" + String(servo_enabled ? "true" : "false") + ",";
  json += "\"humiTask_enabled\":" + String(humiTask_enabled ? "true" : "false");
  json += "},";
  json += "\"enabled_sensors\":" + getEnabledSensorsJson();
  json += "}";
  return json;
}

/**
 * @brief Generates JSON for enabled sensors and their labels.
 */
String getEnabledSensorsJson() {
  String json = "[";
  bool first = true;
  if (humiSensA_enabled) {
    if (!first) json += ",";
    json += "{\"label\":\"" + humiSensA_label + "\",\"pin\":" + String(humiSensA_pin) + "}";
    first = false;
  }
  if (humiSensB_enabled) {
    if (!first) json += ",";
    json += "{\"label\":\"" + humiSensB_label + "\",\"pin\":" + String(humiSensB_pin) + "}";
    first = false;
  }
  json += "]";
  return json;
}

/**
 * @brief Generates JSON for the smooth humidity reading defaults.
 */
String generateSmoothHumiDefaultsJson() {
  String json = "{";
  json += "\"readings\":" + String(smoothRead_defaults_readings) + ",";
  json += "\"interval_ms\":" + String(smoothRead_defaults_interval);
  json += "}";
  return json;
}

// --- Setup and Loop Functions ---

/**
 * @brief Initializes the ESP32 hardware pins and preferences.
 */
void setup(void) {
  Serial.begin(115200);
  Serial.println(F("\nESP32 Booting..."));

  // Load configuration from Preferences
  loadConfig();

  // Initialize GPIOs based on loaded config
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW); // Turn off LED initially

  pinMode(pumpA_pin, OUTPUT);
  digitalWrite(pumpA_pin, LOW);
  pinMode(pumpB_pin, OUTPUT);
  digitalWrite(pumpB_pin, LOW);
  pinMode(pumpC_pin, OUTPUT);
  digitalWrite(pumpC_pin, LOW);
  
  pinMode(humiSensA_pin, INPUT);
  pinMode(humiSensB_pin, INPUT);

  // Removed myServo.attach(servo_pin) from setup(). Servo will be attached only during movement.

  // Start WiFi connection process
  if (savedSSID.length() > 0) {
    Serial.print(F("Found saved credentials. Attempting to connect to "));
    Serial.println(savedSSID);
    currentWiFiState = CONNECTING;
    connectToWiFi();
  } else {
    Serial.println(F("No saved WiFi credentials. Starting provisioning mode."));
    currentWiFiState = PROVISIONING;
    startProvisioningMode();
  }
}

/**
 * @brief Main loop function, handles WiFi state and application tasks.
 */
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
          connectToWiFi(); // Retry connection
        } else {
          Serial.println(F("Max connection retries reached. Clearing credentials and starting provisioning."));
          currentWiFiState = PROVISIONING;
          clearSavedCredentials(); // Clear saved credentials
          startProvisioningMode(); // Go back to provisioning
        }
      }
      break;

    case CONNECTED:
      server.handleClient(); // Handle web server requests
      handlePump();          // Manage pump tasks
      readHumidityTask();    // Read humidity sensors periodically
      handleServo();         // Manage servo movements
      // Small delay to allow other tasks to run
      delay(2);
      break;

    case PROVISIONING:
      // Provisioner handles its own loop internally.
      break;

    case NOT_PROVISIONED:
      // This state should ideally not be reached if setup handles initial state correctly
      break;
  }
}

// --- Configuration Management Functions ---

/**
 * @brief Loads all configuration settings from Preferences (flash memory).
 * Sets default values if a setting is not found.
 */
void loadConfig() {
  preferences.begin("config", true); // 'true' = read-only mode

  // Load Pump Configuration
  pumpA_enabled = preferences.getBool("pumpA_en", true);
  pumpA_pin = preferences.getInt("pumpA_pin", 19);
  pumpB_enabled = preferences.getBool("pumpB_en", true);
  pumpB_pin = preferences.getInt("pumpB_pin", 20);
  pumpC_enabled = preferences.getBool("pumpC_en", true);
  pumpC_pin = preferences.getInt("pumpC_pin", 5);
  max_seconds_on = preferences.getInt("max_sec_on", 10);

  // Load Servo Configuration
  servo_enabled = preferences.getBool("servo_en", true);
  servo_pin = preferences.getInt("servo_pin", 18);
  servo_initAngle = preferences.getInt("servo_init", 90);
  servo_finalAngle = preferences.getInt("servo_final", 150);
  servo_millisMoveDelay = preferences.getInt("servo_delay", 5);

  // Load Humidity Sensor Configuration
  humiSensA_enabled = preferences.getBool("humiA_en", true);
  humiSensA_pin = preferences.getInt("humiA_pin", 35);
  humiSensA_label = preferences.getString("humiA_label", "Sensor A");
  humiSensB_enabled = preferences.getBool("humiB_en", true);
  humiSensB_pin = preferences.getInt("humiB_pin", 32);
  humiSensB_label = preferences.getString("humiB_label", "Sensor B");

  // Load Humidity Task Configuration
  humiTask_enabled = preferences.getBool("humiTask_en", true);
  humiTask_interval = preferences.getLong("humiTask_int", 300);
  
  // Load Smooth Reading Task Defaults
  smoothRead_defaults_readings = preferences.getInt("smooth_readings", 5);
  smoothRead_defaults_interval = preferences.getLong("smooth_interval", 1000);

  // Load Network Configuration
  esp_hostname = preferences.getString("hostname", "esp32");

  // Load WiFi credentials (from "settings" namespace)
  preferences.end(); // Close "config" namespace
  preferences.begin("settings", true); // Open "settings" namespace
  savedSSID = preferences.getString("ssid", "");
  savedPassword = preferences.getString("password", "");
  preferences.end(); // Close "settings" namespace

  Serial.println(F("Configuration loaded."));
}

/**
 * @brief Saves all current configuration settings to Preferences (flash memory).
 */
void saveConfig() {
  preferences.begin("config", false); // read/write mode

  // Save Pump Configuration
  preferences.putBool("pumpA_en", pumpA_enabled);
  preferences.putInt("pumpA_pin", pumpA_pin);
  preferences.putBool("pumpB_en", pumpB_enabled);
  preferences.putInt("pumpB_pin", pumpB_pin);
  preferences.putBool("pumpC_en", pumpC_enabled);
  preferences.putInt("pumpC_pin", pumpC_pin);
  preferences.putInt("max_sec_on", max_seconds_on);

  // Save Servo Configuration
  preferences.putBool("servo_en", servo_enabled);
  preferences.putInt("servo_pin", servo_pin);
  preferences.putInt("servo_init", servo_initAngle);
  preferences.putInt("servo_final", servo_finalAngle);
  preferences.putInt("servo_delay", servo_millisMoveDelay);

  // Save Humidity Sensor Configuration
  preferences.putBool("humiA_en", humiSensA_enabled);
  preferences.putInt("humiA_pin", humiSensA_pin);
  preferences.putString("humiA_label", humiSensA_label);
  preferences.putBool("humiB_en", humiSensB_enabled);
  preferences.putInt("humiB_pin", humiSensB_pin);
  preferences.putString("humiB_label", humiSensB_label);

  // Save Humidity Task Configuration
  preferences.putBool("humiTask_en", humiTask_enabled);
  preferences.putLong("humiTask_int", humiTask_interval);
  
  // Save Smooth Reading Task Defaults
  preferences.putInt("smooth_readings", smoothRead_defaults_readings);
  preferences.putLong("smooth_interval", smoothRead_defaults_interval);

  // Save Network Configuration
  preferences.putString("hostname", esp_hostname);

  preferences.end();
  Serial.println(F("Configuration saved."));
}


// --- WiFi Management Functions ---

/**
 * @brief Attempts to connect to WiFi using saved credentials.
 */
void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
  lastConnectionAttemptMillis = millis();
}

/**
 * @brief Starts the WiFi provisioning mode.
 */
void startProvisioningMode() {
  // Configure the WiFiProvisioner instance
  provisioner.getConfig().SHOW_INPUT_FIELD = false;
  provisioner.getConfig().SHOW_RESET_FIELD = false;

  // Set the success callback for provisioning
  provisioner.onSuccess(
    [](const char *ssid, const char *password, const char *input) {
      Serial.printf("Provisioning successful! Connected to SSID: %s\n", ssid);
      // Save credentials to Preferences (using "settings" namespace)
      preferences.begin("settings", false); // read/write
      preferences.putString("ssid", ssid);
      if (password) {
        preferences.putString("password", password);
      } else {
        preferences.remove("password"); // Clear password if none provided
      }
      preferences.end();

      savedSSID = ssid; // Update global savedSSID
      savedPassword = password ? String(password) : F(""); // Update global savedPassword

      // Set state to CONNECTING, loop() will handle the actual connection check
      currentWiFiState = CONNECTING;
      connectionAttemptsCount = 0; // Reset attempts for new connection
      lastConnectionAttemptMillis = millis(); // Reset timer for new connection
    });

  // Start provisioning
  provisioner.startProvisioning();
  Serial.println(F("WiFi Provisioning started. Connect to AP 'ESP_PROV' and navigate to 192.168.4.1"));
}

/**
 * @brief Clears saved WiFi credentials from Preferences.
 */
void clearSavedCredentials() {
  preferences.begin("settings", false);
  preferences.remove("ssid");
  preferences.remove("password");
  preferences.end();
  savedSSID = F("");
  savedPassword = F("");
  Serial.println(F("Cleared saved WiFi credentials from flash."));
}

/**
 * @brief Initializes web server routes and starts MDNS.
 * This function is called ONLY when WiFi is successfully connected.
 */
void startApplicationServices() {
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());

  // Use the configurable hostname for mDNS
  if (MDNS.begin(esp_hostname.c_str())) {
    Serial.print(F("MDNS responder started at "));
    Serial.print(esp_hostname);
    Serial.println(F(".local"));
  } else {
    Serial.println(F("Error starting MDNS responder!"));
  }

  server.on(F("/"), handleRoot);
  server.on(F("/watering_pumps"), handleWateringPumps);
  server.on(F("/servo_ctrl"), handleServoCtrl);
  server.on(F("/config"), HTTP_GET, handleConfig); // Handle GET for displaying config
  server.on(F("/config"), HTTP_POST, handleConfig); // Handle POST for saving config
  server.on(F("/setMessage"), HTTP_POST, handleSetMessage);
  server.on(F("/api"), handleApi);
  server.on(F("/api/smooth_humi"), HTTP_GET, handleSmoothHumi);
  server.on(F("/api/smooth_humi"), HTTP_POST, handleSmoothHumi);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println(F("HTTP server started"));
}

// --- Application Task Functions ---

/**
 * @brief Handles servo movement based on scheduled flag.
 * Respects servo enable flag and configured parameters.
 * Servo is attached only for the duration of the movement.
 */
void handleServo(){
  if(_global_sched_button && servo_enabled){
    myServo.attach(servo_pin); // Attach only when needed
    Serial.println(F("Moving servo..."));

    for (int angle = servo_initAngle; angle <= servo_finalAngle; angle++) {
      myServo.write(angle);
      delay(servo_millisMoveDelay);
    }

    for (int angle = servo_finalAngle; angle >= servo_initAngle; angle--) {
      myServo.write(angle);
      delay(servo_millisMoveDelay);
    }

    Serial.println(F("Movement complete."));
    _global_sched_button = false;
    myServo.detach(); // Detach to save power and prevent jitter
  }
}

/**
 * @brief Manages pump operation based on scheduled task.
 * Uses configurable pump pins and max_seconds_on.
 */
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

/**
 * @brief Periodically reads humidity sensors.
 * Respects humidity task enable flag and interval.
 */
void readHumidityTask(){
  if (humiTask_enabled) {
    unsigned long _curr_millis = millis();
    if( ( _curr_millis - _previous_millis_read_sensors )>= humiTask_interval){
      readHumidity();
      _previous_millis_read_sensors = _curr_millis;
    }
  }
}

