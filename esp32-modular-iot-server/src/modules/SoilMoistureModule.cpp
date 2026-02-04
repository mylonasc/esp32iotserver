#include "SoilMoistureModule.h"
#include "Html.h"
#include "WebResponse.h"

static uint32_t nowMs() { return millis(); }

void SoilMoistureModule::begin(AppContext& ctx) {
  ctx_ = &ctx;
  
  soil_.begin(ctx.config.soil);
}

void SoilMoistureModule::loop(AppContext& ctx) {
  (void)ctx;
  if (!hasEnabledSensors_()) return;
  soil_.loop();
}

void SoilMoistureModule::renderHome(AppContext& ctx, Print& out) {
  const bool running = hasEnabledSensors_() && ctx.config.soil.intervalMs > 0;
  out.print(F("<div class='box'>"));
  out.print(F("<b>Soil Moisture</b><br>"));

  out.print(F("Running: "));
  out.print(running ? F("Yes") : F("No"));
  out.print(F("<br>"));

  const auto& a = soil_.r0();
  const auto& b = soil_.r1();
  const auto& c = soil_.r2();

  auto addLine = [&](const SoilMoistureController::Reading& r) {
    if (!r.enabled) return;
    out.print(r.id);
    out.print(F(": "));
    if (!r.hasValue) out.print(F("—"));
    else {
      out.print(r.percent);
      out.print(F("%"));
    }
    out.print(F("<br>"));
  };

  addLine(a);
  addLine(b);
  addLine(c);

  out.print(F("<a href='/soil'>Open</a>"));
  out.print(F("</div>"));
}

void SoilMoistureModule::renderConfig(AppContext& ctx, Print& out) {
  auto& sc = ctx.config.soil;

  out.print(F("<details class='box'>"));
  out.print(F("<summary>Soil Moisture</summary>"));
  out.print(F("<label>Read interval (ms)</label>"));
  out.print(F("<input type='number' name='soil_int' min='50' value='"));
  out.print(sc.intervalMs);
  out.print(F("'>"));

  out.print(F("<label>Calibration wetRaw</label>"));
  out.print(F("<input type='number' name='soil_wet' value='"));
  out.print(sc.wetRaw);
  out.print(F("'>"));

  out.print(F("<label>Calibration dryRaw</label>"));
  out.print(F("<input type='number' name='soil_dry' value='"));
  out.print(sc.dryRaw);
  out.print(F("'>"));

  out.print(F("<hr><b>Sensor 1</b><br>"));
  out.print(F("<label><input type='checkbox' name='soil0_en' "));
  if (sc.s0.enabled) out.print(F("checked"));
  out.print(F("> Enabled</label>"));
  out.print(F("<label>Pin</label>"));
  out.print(F("<input type='number' name='soil0_pin' value='"));
  out.print(sc.s0.pin);
  out.print(F("'>"));
  out.print(F("<label>ID</label>"));
  out.print(F("<input name='soil0_id' value='"));
  out.print(sc.s0.id);
  out.print(F("'>"));

  out.print(F("<hr><b>Sensor 2</b><br>"));
  out.print(F("<label><input type='checkbox' name='soil1_en' "));
  if (sc.s1.enabled) out.print(F("checked"));
  out.print(F("> Enabled</label>"));
  out.print(F("<label>Pin</label>"));
  out.print(F("<input type='number' name='soil1_pin' value='"));
  out.print(sc.s1.pin);
  out.print(F("'>"));
  out.print(F("<label>ID</label>"));
  out.print(F("<input name='soil1_id' value='"));
  out.print(sc.s1.id);
  out.print(F("'>"));

  out.print(F("<hr><b>Sensor 3</b><br>"));
  out.print(F("<label><input type='checkbox' name='soil2_en' "));
  if (sc.s2.enabled) out.print(F("checked"));
  out.print(F("> Enabled</label>"));
  out.print(F("<label>Pin</label>"));
  out.print(F("<input type='number' name='soil2_pin' value='"));
  out.print(sc.s2.pin);
  out.print(F("'>"));
  out.print(F("<label>ID</label>"));
  out.print(F("<input name='soil2_id' value='"));
  out.print(sc.s2.id);
  out.print(F("'>"));

  out.print(F("</details>"));
}

void SoilMoistureModule::writeApiStatusObject(AppContext& ctx, Print& out) {
  const bool running = hasEnabledSensors_() && ctx.config.soil.intervalMs > 0;
  out.print(F("{\"api\":\"/api/soil\","));
  out.print(F("\"ui\":\"/soil\","));
  out.print(F("\"running\":"));
  out.print(running ? F("true") : F("false"));
  out.print(F(",\"enabledCount\":"));
  int enabled = 0;
  if (soil_.r0().enabled) enabled++;
  if (soil_.r1().enabled) enabled++;
  if (soil_.r2().enabled) enabled++;
  out.print(enabled);
  out.print(F("}"));
}

void SoilMoistureModule::writeModuleInfoObject(AppContext& ctx, Print& out) {
  (void)ctx;
  out.print(F("{\"name\":\"soil\",\"ui\":\"/soil\",\"api\":\"/api/soil\"}"));
}

void SoilMoistureModule::appendApiStatusObject(AppContext& ctx, String& json) {
  (void)ctx;
  json += "{";
  json += "\"api\":\"/api/soil\",";
  json += "\"ui\":\"/soil\",";
  json += "\"enabledCount\":";
  int enabled = 0;
  if (soil_.r0().enabled) enabled++;
  if (soil_.r1().enabled) enabled++;
  if (soil_.r2().enabled) enabled++;
  json += String(enabled);
  json += "}";
}

void SoilMoistureModule::appendModuleInfoObject(AppContext& ctx, String& json) {
  (void)ctx;
  json += "{";
  json += "\"name\":\"soil\",";
  json += "\"ui\":\"/soil\",";
  json += "\"api\":\"/api/soil\"";
  json += "}";
}

void SoilMoistureModule::registerRoutes(AppContext& ctx) {
  ctx_ = &ctx;
  ctx.server.on("/soil", HTTP_GET, [&ctx, this]() { handleSoilPage_(ctx); });
  ctx.server.on("/api/soil", HTTP_GET, [&ctx, this]() { handleSoilApi_(ctx); });
}

void SoilMoistureModule::renderHome(AppContext& ctx, String& html) {
  (void)ctx;
  const uint32_t now = nowMs();

  html += "<div class='box'>";
  html += "<b>Soil Moisture</b><br>";

  const auto& a = soil_.r0();
  const auto& b = soil_.r1();
  const auto& c = soil_.r2();

  auto addLine = [&](const SoilMoistureController::Reading& r) {
    if (!r.enabled) return;
    html += r.id + ": ";
    if (!r.hasValue) html += "—";
    else html += String(r.percent) + "%";
    html += "<br>";
  };

  addLine(a);
  addLine(b);
  addLine(c);

  html += "<a href='/soil'>Open</a>";
  html += "</div>";
}

void SoilMoistureModule::renderConfig(AppContext& ctx, String& html) {
  auto& sc = ctx.config.soil;

  html += "<div class='box'><h3>Soil Moisture</h3>";
  html += "<label>Read interval (ms)</label>";
  html += "<input type='number' name='soil_int' min='50' value='" + String(sc.intervalMs) + "'>";

  html += "<label>Calibration wetRaw</label>";
  html += "<input type='number' name='soil_wet' value='" + String(sc.wetRaw) + "'>";

  html += "<label>Calibration dryRaw</label>";
  html += "<input type='number' name='soil_dry' value='" + String(sc.dryRaw) + "'>";

  // Sensor 0
  html += "<hr><b>Sensor 1</b><br>";
  html += "<label><input type='checkbox' name='soil0_en' ";
  html += (sc.s0.enabled ? "checked" : "");
  html += "> Enabled</label>";
  html += "<label>Pin</label>";
  html += "<input type='number' name='soil0_pin' value='" + String(sc.s0.pin) + "'>";
  html += "<label>ID</label>";
  html += "<input name='soil0_id' value='" + sc.s0.id + "'>";

  // Sensor 1
  html += "<hr><b>Sensor 2</b><br>";
  html += "<label><input type='checkbox' name='soil1_en' ";
  html += (sc.s1.enabled ? "checked" : "");
  html += "> Enabled</label>";
  html += "<label>Pin</label>";
  html += "<input type='number' name='soil1_pin' value='" + String(sc.s1.pin) + "'>";
  html += "<label>ID</label>";
  html += "<input name='soil1_id' value='" + sc.s1.id + "'>";

  // Sensor 2
  html += "<hr><b>Sensor 3</b><br>";
  html += "<label><input type='checkbox' name='soil2_en' ";
  html += (sc.s2.enabled ? "checked" : "");
  html += "> Enabled</label>";
  html += "<label>Pin</label>";
  html += "<input type='number' name='soil2_pin' value='" + String(sc.s2.pin) + "'>";
  html += "<label>ID</label>";
  html += "<input name='soil2_id' value='" + sc.s2.id + "'>";

  html += "</div>";
}

void SoilMoistureModule::handleConfigPost(AppContext& ctx) {
  auto& s = ctx.server;
  auto& sc = ctx.config.soil;

  if (s.hasArg("soil_int")) {
    long v = s.arg("soil_int").toInt();
    if (v < 50) v = 50;
    sc.intervalMs = (uint32_t)v;
  }
  if (s.hasArg("soil_wet")) sc.wetRaw = s.arg("soil_wet").toInt();
  if (s.hasArg("soil_dry")) sc.dryRaw = s.arg("soil_dry").toInt();

  sc.s0.enabled = s.hasArg("soil0_en");
  sc.s1.enabled = s.hasArg("soil1_en");
  sc.s2.enabled = s.hasArg("soil2_en");

  if (s.hasArg("soil0_pin")) sc.s0.pin = s.arg("soil0_pin").toInt();
  if (s.hasArg("soil1_pin")) sc.s1.pin = s.arg("soil1_pin").toInt();
  if (s.hasArg("soil2_pin")) sc.s2.pin = s.arg("soil2_pin").toInt();

  if (s.hasArg("soil0_id")) sc.s0.id = s.arg("soil0_id");
  if (s.hasArg("soil1_id")) sc.s1.id = s.arg("soil1_id");
  if (s.hasArg("soil2_id")) sc.s2.id = s.arg("soil2_id");

  soil_.begin(sc);
}


void SoilMoistureModule::appendSensorRow_(Print& out, const SoilMoistureController::Reading& r, uint32_t now) {
  if (!r.enabled) return;

  out.print(F("<tr>"));
  out.print(F("<td>"));
  out.print(r.id);
  out.print(F("</td>"));
  out.print(F("<td>"));
  out.print(r.pin);
  out.print(F("</td>"));

  if (!r.hasValue) {
    out.print(F("<td>—</td><td>—</td><td>—</td>"));
  } else {
    out.print(F("<td>"));
    out.print(r.raw);
    out.print(F("</td>"));
    out.print(F("<td><b>"));
    out.print(r.percent);
    out.print(F("%</b></td>"));
    out.print(F("<td>"));
    out.print((uint32_t)((now - r.lastReadMs) / 1000));
    out.print(F("s</td>"));
  }
  out.print(F("</tr>"));
}

void SoilMoistureModule::handleSoilPage_(AppContext& ctx) {
  const uint32_t now = nowMs();
  auto res = beginChunkedHtml(ctx.server, F("Soil Moisture"));
  auto& out = res.out();
  out.print(F("<h2>Soil Moisture</h2>"));

  out.print(F("<div class='box'>"));
  out.print(F("<b>Interval:</b> "));
  out.print(ctx.config.soil.intervalMs);
  out.print(F(" ms<br>"));
  out.print(F("<b>Calibration:</b> wetRaw="));
  out.print(ctx.config.soil.wetRaw);
  out.print(F(", dryRaw="));
  out.print(ctx.config.soil.dryRaw);
  out.print(F("<br><a href='/api/soil'>JSON</a>"));
  out.print(F("</div>"));

  out.print(F("<table style='width:100%;border-collapse:collapse;'>"));
  out.print(F("<tr><th align='left'>ID</th><th align='left'>Pin</th><th align='left'>Raw</th><th align='left'>Moisture</th><th align='left'>Age</th></tr>"));

  appendSensorRow_(out, soil_.r0(), now);
  appendSensorRow_(out, soil_.r1(), now);
  appendSensorRow_(out, soil_.r2(), now);

  out.print(F("</table>"));
  out.print(F("<p><button type='button' onclick='location.reload()'>Refresh</button></p>"));
  out.print(htmlFooter());
}

static void printJsonString_(Print& out, const String& in) {
  for (size_t i = 0; i < in.length(); ++i) {
    char c = in[i];
    if (c == '"') out.print(F("\\\""));
    else if (c == '\\') out.print(F("\\\\"));
    else out.print(c);
  }
}

static void printJsonStringQuoted_(Print& out, const String& in) {
  out.print('"');
  printJsonString_(out, in);
  out.print('"');
}

static void writeSoilSensor_(Print& out, const SoilMoistureController::Reading& r, uint32_t now) {
  out.print(F("{"));
  out.print(F("\"enabled\":")); out.print(r.enabled ? F("true") : F("false"));
  out.print(F(",\"id\":")); printJsonStringQuoted_(out, r.id);
  out.print(F(",\"pin\":")); out.print(r.pin);
  out.print(F(",\"hasValue\":")); out.print(r.hasValue ? F("true") : F("false"));
  out.print(F(",\"raw\":")); out.print(r.hasValue ? r.raw : 0);
  out.print(F(",\"percent\":")); out.print(r.hasValue ? r.percent : 0);
  out.print(F(",\"ageMs\":")); out.print(r.hasValue ? (uint32_t)(now - r.lastReadMs) : 0);
  out.print(F("}"));
}

void SoilMoistureModule::appendSensorJson_(Print& out, const SoilMoistureController::Reading& r, uint32_t now) {
  out.print(F("{"));
  out.print(F("\"enabled\":"));
  out.print(r.enabled ? F("true") : F("false"));
  out.print(F(",\"id\":\""));
  printJsonString_(out, r.id);
  out.print(F("\","));
  out.print(F("\"pin\":"));
  out.print(r.pin);
  out.print(F(",\"hasValue\":"));
  out.print(r.hasValue ? F("true") : F("false"));
  out.print(F(",\"raw\":"));
  out.print(r.hasValue ? r.raw : 0);
  out.print(F(",\"percent\":"));
  out.print(r.hasValue ? r.percent : 0);
  out.print(F(",\"ageMs\":"));
  out.print(r.hasValue ? (uint32_t)(now - r.lastReadMs) : 0);
  out.print(F("}"));
}

void SoilMoistureModule::handleSoilApi_(AppContext& ctx) {
  const uint32_t now = nowMs();
  auto res = beginChunkedJson(ctx.server);
  auto& out = res.out();
  out.print(F("{"));
  out.print(F("\"intervalMs\":"));
  out.print(ctx.config.soil.intervalMs);
  out.print(F(",\"wetRaw\":"));
  out.print(ctx.config.soil.wetRaw);
  out.print(F(",\"dryRaw\":"));
  out.print(ctx.config.soil.dryRaw);
  out.print(F(",\"sensors\":["));

  appendSensorJson_(out, soil_.r0(), now); out.print(F(","));
  appendSensorJson_(out, soil_.r1(), now); out.print(F(","));
  appendSensorJson_(out, soil_.r2(), now);

  out.print(F("]}"));
}

void SoilMoistureModule::appendMcpTools(Print& out, bool& first) {
  auto addTool = [&](const __FlashStringHelper* name,
                     const __FlashStringHelper* description,
                     const __FlashStringHelper* schema) {
    if (!first) out.print(',');
    first = false;
    out.print(F("{\"name\":"));
    out.print('"'); out.print(name); out.print(F("\","));
    out.print(F("\"description\":"));
    out.print('"'); out.print(description); out.print(F("\","));
    out.print(F("\"inputSchema\":"));
    out.print(schema);
    out.print(F("}"));
  };

  addTool(F("soil.readings"),
          F("Get soil sensor readings with calibration and running state."),
          F("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"));

  addTool(F("soil.config.get"),
          F("Get soil sensor configuration (interval, calibration, sensors)."),
          F("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"));

  addTool(F("soil.config.set"),
          F("Update soil configuration. Can save/apply and request reboot."),
          F("{\"type\":\"object\",\"properties\":{"
            "\"intervalMs\":{\"type\":\"integer\"},\"wetRaw\":{\"type\":\"integer\"},\"dryRaw\":{\"type\":\"integer\"},"
            "\"s0\":{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"},\"pin\":{\"type\":\"integer\"},\"id\":{\"type\":\"string\"}},\"additionalProperties\":false},"
            "\"s1\":{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"},\"pin\":{\"type\":\"integer\"},\"id\":{\"type\":\"string\"}},\"additionalProperties\":false},"
            "\"s2\":{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"},\"pin\":{\"type\":\"integer\"},\"id\":{\"type\":\"string\"}},\"additionalProperties\":false},"
            "\"save\":{\"type\":\"boolean\"},\"apply\":{\"type\":\"boolean\"},\"reboot\":{\"type\":\"boolean\"}"
          "},\"additionalProperties\":false}"));
}

bool SoilMoistureModule::supportsMcpTool(const char* toolName) const {
  return strcmp(toolName, "soil.readings") == 0 ||
         strcmp(toolName, "soil.config.get") == 0 ||
         strcmp(toolName, "soil.config.set") == 0;
}

bool SoilMoistureModule::handleMcpToolCall(AppContext& ctx,
                                           const char* toolName,
                                           JsonObject args,
                                           Print& out,
                                           bool& rebootRequested) {
  if (strcmp(toolName, "soil.readings") == 0) {
    const uint32_t now = millis();
    const auto& cfg = ctx.config.soil;
    const bool running = hasEnabledSensors_() && cfg.intervalMs > 0;

    out.print(F("{"));
    out.print(F("\"running\":")); out.print(running ? F("true") : F("false"));
    out.print(F(",\"intervalMs\":")); out.print(cfg.intervalMs);
    out.print(F(",\"wetRaw\":")); out.print(cfg.wetRaw);
    out.print(F(",\"dryRaw\":")); out.print(cfg.dryRaw);
    out.print(F(",\"sensors\":["));
    writeSoilSensor_(out, soil_.r0(), now); out.print(',');
    writeSoilSensor_(out, soil_.r1(), now); out.print(',');
    writeSoilSensor_(out, soil_.r2(), now);
    out.print(F("]}"));
    return true;
  }

  if (strcmp(toolName, "soil.config.get") == 0) {
    const auto& cfg = ctx.config.soil;
    out.print(F("{"));
    out.print(F("\"intervalMs\":")); out.print(cfg.intervalMs);
    out.print(F(",\"wetRaw\":")); out.print(cfg.wetRaw);
    out.print(F(",\"dryRaw\":")); out.print(cfg.dryRaw);
    out.print(F(",\"s0\":{"));
  out.print(F("\"enabled\":")); out.print(cfg.s0.enabled ? F("true") : F("false"));
  out.print(F(",\"pin\":")); out.print(cfg.s0.pin);
  out.print(F(",\"id\":")); printJsonStringQuoted_(out, cfg.s0.id);
  out.print(F("},\"s1\":{"));
  out.print(F("\"enabled\":")); out.print(cfg.s1.enabled ? F("true") : F("false"));
  out.print(F(",\"pin\":")); out.print(cfg.s1.pin);
  out.print(F(",\"id\":")); printJsonStringQuoted_(out, cfg.s1.id);
  out.print(F("},\"s2\":{"));
  out.print(F("\"enabled\":")); out.print(cfg.s2.enabled ? F("true") : F("false"));
  out.print(F(",\"pin\":")); out.print(cfg.s2.pin);
  out.print(F(",\"id\":")); printJsonStringQuoted_(out, cfg.s2.id);
    out.print(F("}}"));
    return true;
  }

  if (strcmp(toolName, "soil.config.set") == 0) {
    const bool save = !args.containsKey("save") || args["save"].as<bool>();
    const bool apply = !args.containsKey("apply") || args["apply"].as<bool>();
    const bool reboot = args.containsKey("reboot") && args["reboot"].as<bool>();

    if (args.containsKey("intervalMs")) {
      long v = args["intervalMs"].as<long>();
      if (v < 50) v = 50;
      ctx.config.soil.intervalMs = (uint32_t)v;
    }
    if (args.containsKey("wetRaw")) ctx.config.soil.wetRaw = args["wetRaw"].as<int>();
    if (args.containsKey("dryRaw")) ctx.config.soil.dryRaw = args["dryRaw"].as<int>();

    JsonObject s0 = args["s0"].as<JsonObject>();
    if (!s0.isNull()) {
      if (s0.containsKey("enabled")) ctx.config.soil.s0.enabled = s0["enabled"].as<bool>();
      if (s0.containsKey("pin")) ctx.config.soil.s0.pin = s0["pin"].as<int>();
      if (s0.containsKey("id")) ctx.config.soil.s0.id = s0["id"].as<const char*>();
    }
    JsonObject s1 = args["s1"].as<JsonObject>();
    if (!s1.isNull()) {
      if (s1.containsKey("enabled")) ctx.config.soil.s1.enabled = s1["enabled"].as<bool>();
      if (s1.containsKey("pin")) ctx.config.soil.s1.pin = s1["pin"].as<int>();
      if (s1.containsKey("id")) ctx.config.soil.s1.id = s1["id"].as<const char*>();
    }
    JsonObject s2 = args["s2"].as<JsonObject>();
    if (!s2.isNull()) {
      if (s2.containsKey("enabled")) ctx.config.soil.s2.enabled = s2["enabled"].as<bool>();
      if (s2.containsKey("pin")) ctx.config.soil.s2.pin = s2["pin"].as<int>();
      if (s2.containsKey("id")) ctx.config.soil.s2.id = s2["id"].as<const char*>();
    }

    if (save) ctx.configStore.save(ctx.config);
    if (apply) soil_.begin(ctx.config.soil);
    if (reboot) rebootRequested = true;

    out.print(F("{\"saved\":")); out.print(save ? F("true") : F("false"));
    out.print(F(",\"applied\":")); out.print(apply ? F("true") : F("false"));
    out.print(F(",\"rebooting\":")); out.print(reboot ? F("true") : F("false"));
    out.print(F("}"));
    return true;
  }

  return false;
}

bool SoilMoistureModule::hasEnabledSensors_() const {
  return soil_.r0().enabled || soil_.r1().enabled || soil_.r2().enabled;
}
