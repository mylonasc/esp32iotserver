#include "WebUi.h"
#include "WebServerPrint.h"
#include <WiFi.h>
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

void WebUi::handleConfigGet_(AppContext& ctx) {
  HtmlOut out(ctx.server);

  out.begin(F("Config"));
  out.p(F("<h2>Configuration</h2>"));
  out.p(F("<form method='POST' action='/config'>"));
  out.p(F("<div class='box'><h3>Network Settings</h3>"));

  auto renderInput = [&](const char* label, const char* name, String value, bool isPass = false) {
    out.p(F("<label>")); out.p(label); out.p(F("</label><br>"));
    out.p(F("<input name='")); out.p(name); 
    if(isPass) out.p(F("' type='password"));
    out.p(F("' value='")); out.p(value); out.p(F("'><br><br>"));
  };

  renderInput("Hostname (.local)", "hostname", ctx.config.hostname);
  renderInput("WiFi SSID", "ssid", ctx.config.ssid);
  renderInput("WiFi Password", "password", ctx.config.password, true);

  out.p(F("</div>"));

  WebServerPrint mout(ctx.server);
  modules_.renderConfig(ctx, mout);

  out.p(F("<button type='submit' style='padding:10px 20px;'>Save Configuration</button>"));
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
  ctx.config.hostname = ctx.server.arg("hostname");
  ctx.config.ssid     = ctx.server.arg("ssid");
  ctx.config.password = ctx.server.arg("password");

  // 3. Let modules update their parts
  // We pass the reference to ensure they use the newly allocated modules
  modules_.handleConfigPost(ctx);

  // 4. Persist to NVS
  ctx.configStore.save(ctx.config);

  // 5. Success Response
  ctx.server.send(200, "text/plain", "Saved successfully. Rebooting...");
  
  Serial.println(F("Config saved. Triggering reboot..."));
  delay(500);
  ESP.restart();
}

void WebUi::handleResetWifi_(AppContext& ctx) {
  // Clear the NVS credentials
  ctx.configStore.clearWifiCredentials();

  ctx.server.send(200, "text/plain", "WiFi credentials cleared. Rebooting to Provisioning Mode...");
  
  Serial.println(F("WiFi Reset requested. Rebooting..."));
  delay(500);
  ESP.restart();
}