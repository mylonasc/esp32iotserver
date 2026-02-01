#include "PumpModule.h"
#include "Html.h"

void PumpModule::begin(AppContext& ctx) {
  ctx_ = &ctx;
  pumps_.begin(ctx.config.pumps);
}

void PumpModule::loop(AppContext& ctx) {
  (void)ctx;
  pumps_.loop();
}

void PumpModule::registerRoutes(AppContext& ctx) {
  ctx_ = &ctx;
  ctx.server.on("/watering_pumps", HTTP_GET, [this]() { handlePumpsPage_(*ctx_); });
  ctx.server.on("/api/pumps", HTTP_GET, [this]() { handlePumpsApi_(*ctx_); });
  ctx.server.on("/watering_pumps", HTTP_GET, [&ctx, this]() { handlePumpsPage_(ctx); });
  ctx.server.on("/api/pumps", HTTP_GET, [&ctx, this]() { handlePumpsApi_(ctx); });
}

void PumpModule::renderHome(AppContext& ctx, String& html) {
  (void)ctx;
  html += "<div class='box'>";
  html += "<b>Pumps</b><br>";
  html += "Running: " + String(pumps_.isRunning() ? "Yes" : "No");
  html += "<br>Remaining: " + String(pumps_.remainingSeconds(), 1) + " s";
  html += "<br><a href='/watering_pumps'>Open</a>";
  html += "</div>";
}

void PumpModule::renderConfig(AppContext& ctx, String& html) {
  auto& p = ctx.config.pumps;

  html += "<div class='box'><h3>Pumps</h3>";

  html += "<label><input type='checkbox' name='pumpA_en' ";
  html += (p.enabledA ? "checked" : "");
  html += "> Enable Pump A</label>";
  html += "<label>Pump A pin</label>";
  html += "<input type='number' name='pumpA_pin' value='" + String(p.pinA) + "'>";

  html += "<label><input type='checkbox' name='pumpB_en' ";
  html += (p.enabledB ? "checked" : "");
  html += "> Enable Pump B</label>";
  html += "<label>Pump B pin</label>";
  html += "<input type='number' name='pumpB_pin' value='" + String(p.pinB) + "'>";

  html += "<label><input type='checkbox' name='pumpC_en' ";
  html += (p.enabledC ? "checked" : "");
  html += "> Enable Pump C</label>";
  html += "<label>Pump C pin</label>";
  html += "<input type='number' name='pumpC_pin' value='" + String(p.pinC) + "'>";

  html += "<label>Max seconds on</label>";
  html += "<input type='number' name='pump_max' min='1' max='600' value='" + String(p.maxSecondsOn) + "'>";

  html += "</div>";
}

void PumpModule::appendApiStatusObject(AppContext& ctx, String& json) {
  (void)ctx;
  json += "{";
  json += "\"running\":";
  json += (pumps_.isRunning() ? "true" : "false");
  json += ",";
  json += "\"activePin\":";
  json += String(pumps_.activePin());
  json += ",";
  json += "\"remainingSeconds\":";
  json += String(pumps_.remainingSeconds(), 2);
  json += "}";
}

void PumpModule::appendModuleInfoObject(AppContext& ctx, String& json) {
  (void)ctx;
  json += "{";
  json += "\"name\":\"pumps\",";
  json += "\"ui\":\"/watering_pumps\",";
  json += "\"api\":\"/api/pumps\"";
  json += "}";
}

void PumpModule::handleConfigPost(AppContext& ctx) {
  auto& s = ctx.server;
  auto& p = ctx.config.pumps;

  p.enabledA = s.hasArg("pumpA_en");
  p.enabledB = s.hasArg("pumpB_en");
  p.enabledC = s.hasArg("pumpC_en");

  if (s.hasArg("pumpA_pin")) p.pinA = s.arg("pumpA_pin").toInt();
  if (s.hasArg("pumpB_pin")) p.pinB = s.arg("pumpB_pin").toInt();
  if (s.hasArg("pumpC_pin")) p.pinC = s.arg("pumpC_pin").toInt();

  if (s.hasArg("pump_max")) {
    int v = s.arg("pump_max").toInt();
    if (v < 1) v = 1;
    p.maxSecondsOn = v;
  }

  // Apply immediately (so after reboot or even before, config is consistent)
  pumps_.begin(p);
}

void PumpModule::handlePumpsPage_(AppContext& ctx) {
  String html = htmlHeader("Pumps");
  html += "<h2>Watering Pumps</h2>";

  html += "<div class='box'>";
  html += "<b>Running:</b> " + String(pumps_.isRunning() ? "Yes" : "No");
  html += "<br><b>Active pin:</b> " + String(pumps_.activePin());
  html += "<br><b>Remaining:</b> " + String(pumps_.remainingSeconds(), 1) + " s";
  html += "</div>";

  html += "<form method='GET' action='/watering_pumps'>";
  html += "<label>Seconds (max " + String(ctx.config.pumps.maxSecondsOn) + ")</label>";
  html += "<input type='number' name='sec' min='1' max='" + String(ctx.config.pumps.maxSecondsOn) + "' value='5'>";

  if (ctx.config.pumps.enabledA) html += "<button name='ch' value='A'>Pump A</button>";
  if (ctx.config.pumps.enabledB) html += "<button name='ch' value='B'>Pump B</button>";
  if (ctx.config.pumps.enabledC) html += "<button name='ch' value='C'>Pump C</button>";
  html += "<button name='off' value='1'>All Off</button>";
  html += "</form>";

  // Action handling
  if (ctx.server.hasArg("off")) {
    pumps_.allOff();
    html += "<p><b>OK:</b> All pumps off.</p>";
  } else if (ctx.server.hasArg("ch") && ctx.server.hasArg("sec")) {
    String chStr = ctx.server.arg("ch");
    char ch = chStr.length() ? chStr[0] : ' ';
    int sec = ctx.server.arg("sec").toInt();

    bool ok = pumps_.start(ch, sec);
    if (ok) {
      html += "<p><b>OK:</b> Started pump ";
      html += ch;
      html += " for ";
      html += sec;
      html += " seconds.</p>";
    } else {
      html += "<p style='color:red;'><b>Error:</b> Could not start pump. "
              "It may be disabled or already running.</p>";
    }
  }

  html += htmlFooter();
  ctx.server.send(200, "text/html", html);
}

void PumpModule::handlePumpsApi_(AppContext& ctx) {
  String json = "{";
  json += "\"running\":" + String(pumps_.isRunning() ? "true" : "false") + ",";
  json += "\"activePin\":" + String(pumps_.activePin()) + ",";
  json += "\"remainingSeconds\":" + String(pumps_.remainingSeconds(), 2);
  json += "}";
  ctx.server.send(200, "application/json", json);
}
