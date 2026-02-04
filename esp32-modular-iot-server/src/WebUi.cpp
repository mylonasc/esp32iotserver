#include "WebUi.h"
#include "WebServerPrint.h"
#include <WiFi.h>
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Html.h"

// Improved HtmlOut: Uses internal WebServer methods more directly
class HtmlOut {
public:
  explicit HtmlOut(WebServer& server) : s_(server) {}

  void begin(const __FlashStringHelper* title) {
    s_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    s_.sendHeader(F("Cache-Control"), F("no-cache"));
    s_.send(200, "text/html", "");
    // Ensure htmlHeader is also returning __FlashStringHelper* or is in PROGMEM
    s_.sendContent(htmlHeader(title)); 
  }

  void end() {
    s_.sendContent(htmlFooter());
    s_.sendContent(""); // Explicitly send the zero-chunk to close the stream
  }

  // Use the FlashStringHelper overload primarily
  void p(const __FlashStringHelper* t) { s_.sendContent(t); }
  void p(const String& t)              { s_.sendContent(t); }
  void p(const char* t)                { s_.sendContent(t); }

private:
  WebServer& s_;
};

void WebUi::registerRoutes(AppContext& ctx) {
  // Capture ctx by reference directly into the lambda
  // This is safer than relying on a class member pointer which can be corrupted
  ctx.server.on("/", HTTP_GET, [this, &ctx]() { 
    if (WiFi.status() != WL_CONNECTED) return;
    this->handleRoot_(ctx); 
  });

  ctx.server.on("/diag", HTTP_GET, [this, &ctx]() {
    this->handleDiagnostics_(ctx);
  });

  ctx.server.on("/config", HTTP_GET, [this, &ctx]() { 
    this->handleConfigGet_(ctx); 
  });

  ctx.server.on("/config", HTTP_POST, [this, &ctx]() { 
    this->handleConfigPost_(ctx); 
  });
}

void WebUi::handleRoot_(AppContext& ctx) {
  HtmlOut out(ctx.server);

  out.begin(F("Home"));
  out.p(F("<style>.box{padding:10px;border:1px solid #ccc;margin:10px 0;}</style>")); // Minified CSS
  out.p(F("<h2>ESP32 Modular</h2>"));

  out.p(F("<div class='box'>"));
  out.p(F("<b>WiFi Status:</b> "));
  out.p(WiFi.status() == WL_CONNECTED ? F("Connected") : F("Disconnected"));

  if (WiFi.status() == WL_CONNECTED) {
    out.p(F("<br><b>IP Address:</b> "));
    out.p(WiFi.localIP().toString());
  }

  out.p(F("<br><b>Hostname:</b> "));
  out.p(ctx.config.hostname);
  out.p(F("</div>"));

  // Pass ctx by reference to modules
  WebServerPrint mout(ctx.server);
  modules_.renderHome(ctx, mout); 

  out.end();
}

void WebUi::handleDiagnostics_(AppContext& ctx) {
  HtmlOut out(ctx.server);
  const uint32_t uptimeSec = millis() / 1000;
  const size_t freeHeap = ESP.getFreeHeap();
  const size_t minFreeHeap = ESP.getMinFreeHeap();
  const size_t maxAllocHeap = ESP.getMaxAllocHeap();
  const size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  const size_t flashSize = ESP.getFlashChipSize();
  const size_t sketchSize = ESP.getSketchSize();
  const size_t freeSketch = ESP.getFreeSketchSpace();
  const UBaseType_t taskCount = uxTaskGetNumberOfTasks();

  float fragmentation = 0.0f;
  if (freeHeap > 0) {
    fragmentation = 100.0f - (largestBlock * 100.0f / freeHeap);
  }

  out.begin(F("Diagnostics"));
  out.p(F("<h2>Diagnostics</h2>"));

  out.p(F("<div class='box'>"));
  out.p(F("<b>Uptime:</b> ")); out.p(String(uptimeSec)); out.p(F(" s<br>"));
  out.p(F("<b>Tasks:</b> ")); out.p(String(taskCount)); out.p(F("<br>"));
  out.p(F("<b>WiFi:</b> ")); out.p(WiFi.status() == WL_CONNECTED ? F("Connected") : F("Disconnected"));
  if (WiFi.status() == WL_CONNECTED) {
    out.p(F("<br><b>IP:</b> ")); out.p(WiFi.localIP().toString());
  }
  out.p(F("</div>"));

  out.p(F("<div class='box'>"));
  out.p(F("<b>Heap</b><br>"));
  out.p(F("Free: ")); out.p(String(freeHeap)); out.p(F(" bytes<br>"));
  out.p(F("Min free: ")); out.p(String(minFreeHeap)); out.p(F(" bytes<br>"));
  out.p(F("Max alloc: ")); out.p(String(maxAllocHeap)); out.p(F(" bytes<br>"));
  out.p(F("Largest block: ")); out.p(String(largestBlock)); out.p(F(" bytes<br>"));
  out.p(F("Fragmentation: ")); out.p(String(fragmentation, 2)); out.p(F("%"));
  out.p(F("</div>"));

  out.p(F("<div class='box'>"));
  out.p(F("<b>Flash</b><br>"));
  out.p(F("Chip size: ")); out.p(String(flashSize)); out.p(F(" bytes<br>"));
  out.p(F("Sketch size: ")); out.p(String(sketchSize)); out.p(F(" bytes<br>"));
  out.p(F("Free sketch: ")); out.p(String(freeSketch)); out.p(F(" bytes"));
  out.p(F("</div>"));

  out.p(F("<p><button type='button' onclick='location.reload()'>Refresh</button></p>"));
  out.end();
}

void WebUi::handleConfigGet_(AppContext& ctx) {
  HtmlOut out(ctx.server);

  out.begin(F("Config"));
  out.p(F("<h2>Configuration</h2>"));

  if (ctx.server.hasArg("saved")) {
    out.p(F("<div class='box' style='background:#e9f7ef;border-color:#b7e1c5;'>"));
    out.p(F("<b>Applied OK.</b> Settings saved without reboot."));
    out.p(F("</div>"));
  } else if (ctx.server.hasArg("reboot")) {
    out.p(F("<div class='box' style='background:#fff4e5;border-color:#f5c26b;'>"));
    out.p(F("<b>Saved.</b> Rebooting..."));
    out.p(F("</div>"));
  }

  out.p(F("<form method='POST' action='/config'>"));
  out.p(F("<details class='box'>"));
  out.p(F("<summary>Network Settings</summary>"));

  auto renderInput = [&](const char* label, const char* name, String value, bool isPass = false) {
    out.p(F("<label>")); out.p(label); out.p(F("</label><br>"));
    out.p(F("<input name='")); out.p(name); 
    if(isPass) out.p(F("' type='password"));
    out.p(F("' value='")); out.p(value); out.p(F("'><br><br>"));
  };

  renderInput("Hostname (.local)", "hostname", ctx.config.hostname);
  renderInput("WiFi SSID", "ssid", ctx.config.ssid);
  renderInput("WiFi Password", "password", ctx.config.password, true);

  out.p(F("</details>"));

  WebServerPrint mout(ctx.server);
  modules_.renderConfig(ctx, mout);

  out.p(F("<button type='submit' name='action' value='save' style='padding:10px 20px;'>Save (Apply)</button>"));
  out.p(F("<button type='submit' name='action' value='reboot' style='padding:10px 20px;'>Save & Reboot</button>"));
  out.p(F("</form>"));
  out.p(F("<hr><form method='POST' action='/reset_wifi'><button type='submit'>Reset WiFi</button></form>"));

  out.end();
}

void WebUi::handleConfigPost_(AppContext& ctx) {
  // 1. Safety check
  if (!ctx.server.hasArg("hostname")) {
    ctx.server.send(400, "text/plain", "Missing args");
    return;
  }

  // 2. Core fields: Using .arg() creates a temporary String. 
  // We assign it immediately to the config strings.
  const String prevHostname = ctx.config.hostname;
  const String prevSsid = ctx.config.ssid;
  const String prevPassword = ctx.config.password;

  ctx.config.hostname = ctx.server.arg("hostname");
  ctx.config.ssid     = ctx.server.arg("ssid");
  ctx.config.password = ctx.server.arg("password");

  // 3. Let modules update their parts
  // We pass the reference to ensure they use the newly allocated modules
  modules_.handleConfigPost(ctx);

  // 4. Persist to NVS
  ctx.configStore.save(ctx.config);

  const String action = ctx.server.hasArg("action") ? ctx.server.arg("action") : String("save");
  const bool doReboot = (action == "reboot");
  const bool wifiChanged = (ctx.config.ssid != prevSsid) || (ctx.config.password != prevPassword);
  const bool hostnameChanged = (ctx.config.hostname != prevHostname);

  if (!doReboot && (wifiChanged || hostnameChanged)) {
    ctx.wifi.begin(ctx.config, ctx.configStore);
  }

  // 5. Success Response (redirect back to /config)
  const char* location = doReboot ? "/config?reboot=1" : "/config?saved=1";
  ctx.server.sendHeader("Location", location);
  ctx.server.send(303, "text/plain", "");

  if (doReboot) {
    Serial.println(F("Config saved. Rebooting by request..."));
    delay(300);
    ESP.restart();
  } else {
    Serial.println(F("Config saved. Applying changes without reboot."));
  }
}

void WebUi::handleResetWifi_(AppContext& ctx) {
  // Clear the NVS credentials
  ctx.configStore.clearWifiCredentials();

  ctx.server.send(200, "text/plain", "WiFi credentials cleared. Rebooting to Provisioning Mode...");
  
  Serial.println(F("WiFi Reset requested. Rebooting..."));
  delay(500);
  ESP.restart();
}
