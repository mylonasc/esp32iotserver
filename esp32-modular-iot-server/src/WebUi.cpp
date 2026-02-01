#include "WebUi.h"
#include <WiFi.h>
#include "Html.h"

void WebUi::registerRoutes(AppContext& ctx) {
  ctx.server.on("/", HTTP_GET, [&ctx, this]() { handleRoot_(ctx); });

  ctx.server.on("/config", HTTP_GET, [&ctx, this]() { handleConfigGet_(ctx); });
  ctx.server.on("/config", HTTP_POST, [&ctx, this]() { handleConfigPost_(ctx); });

  ctx.server.on("/reset_wifi", HTTP_POST, [&ctx, this]() { handleResetWifi_(ctx); });
}

void WebUi::handleRoot_(AppContext& ctx) {
  String s = htmlHeader("Home");
  s += "<h2>ESP32 Modular</h2>";

  s += "<div class='box'>";
  s += "<b>WiFi:</b> ";
  s += (WiFi.status() == WL_CONNECTED) ? "Connected" : "Not connected";
  if (WiFi.status() == WL_CONNECTED) {
    s += "<br><b>IP:</b> " + WiFi.localIP().toString();
  }
  s += "<br><b>Hostname:</b> " + ctx.config.hostname;
  s += "</div>";

  // Modules add their own status boxes
  modules_.renderHome(ctx, s);

  s += htmlFooter();
  ctx.server.send(200, "text/html", s);
}

void WebUi::handleConfigGet_(AppContext& ctx) {
  String s = htmlHeader("Config");
  s += "<h2>Configuration</h2>";

  s += "<form method='POST' action='/config'>";

  s += "<div class='box'><h3>Network</h3>";
  s += "<label>Hostname (.local)</label>";
  s += "<input name='hostname' value='" + ctx.config.hostname + "'>";

  s += "<label>WiFi SSID</label>";
  s += "<input name='ssid' value='" + ctx.config.ssid + "'>";

  s += "<label>WiFi Password</label>";
  s += "<input name='password' type='password' value='" + ctx.config.password + "'>";
  s += "</div>";

  // Modules add their own config fields
  modules_.renderConfig(ctx, s);

  s += "<button type='submit'>Save</button>";
  s += "</form>";

  s += "<hr>";
  s += "<form method='POST' action='/reset_wifi'>";
  s += "<button type='submit'>Reset WiFi & Reboot</button>";
  s += "</form>";

  s += htmlFooter();
  ctx.server.send(200, "text/html", s);
}

void WebUi::handleConfigPost_(AppContext& ctx) {
  // Core fields
  if (ctx.server.hasArg("hostname")) ctx.config.hostname = ctx.server.arg("hostname");
  if (ctx.server.hasArg("ssid")) ctx.config.ssid = ctx.server.arg("ssid");
  if (ctx.server.hasArg("password")) ctx.config.password = ctx.server.arg("password");

  // Let modules update their parts
  modules_.handleConfigPost(ctx);

  // Persist once
  ctx.configStore.save(ctx.config);

  // Simple + reliable: reboot so hostname/mDNS/modules re-init cleanly
  ctx.server.send(200, "text/plain", "Saved. Rebooting...");
  delay(300);
  ESP.restart();
}

void WebUi::handleResetWifi_(AppContext& ctx) {
  ctx.server.send(200, "text/plain", "WiFi reset. Rebooting into provisioning...");
  delay(300);
  ctx.configStore.clearWifiCredentials();
  ESP.restart();
}
