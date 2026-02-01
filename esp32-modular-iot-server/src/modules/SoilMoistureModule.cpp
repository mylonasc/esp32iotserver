#include "SoilMoistureModule.h"
#include "Html.h"

static uint32_t nowMs() { return millis(); }

void SoilMoistureModule::begin(AppContext& ctx) {
  soil_.begin(ctx.config.soil);
}

void SoilMoistureModule::loop(AppContext& ctx) {
  (void)ctx;
  soil_.loop();
}

void SoilMoistureModule::registerRoutes(AppContext& ctx) {
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


void SoilMoistureModule::appendSensorRow_(String& html, const SoilMoistureController::Reading& r, uint32_t now) {
  if (!r.enabled) return;

  html += "<tr>";
  html += "<td>" + r.id + "</td>";
  html += "<td>" + String(r.pin) + "</td>";

  if (!r.hasValue) {
    html += "<td>—</td><td>—</td><td>—</td>";
  } else {
    html += "<td>" + String(r.raw) + "</td>";
    html += "<td><b>" + String(r.percent) + "%</b></td>";
    html += "<td>" + String((now - r.lastReadMs) / 1000) + "s</td>";
  }
  html += "</tr>";
}

void SoilMoistureModule::handleSoilPage_(AppContext& ctx) {
  const uint32_t now = nowMs();
  String html = htmlHeader("Soil Moisture");
  html += "<h2>Soil Moisture</h2>";

  html += "<div class='box'>";
  html += "<b>Interval:</b> " + String(ctx.config.soil.intervalMs) + " ms<br>";
  html += "<b>Calibration:</b> wetRaw=" + String(ctx.config.soil.wetRaw) +
          ", dryRaw=" + String(ctx.config.soil.dryRaw);
  html += "<br><a href='/api/soil'>JSON</a>";
  html += "</div>";

  html += "<table style='width:100%;border-collapse:collapse;'>";
  html += "<tr><th align='left'>ID</th><th align='left'>Pin</th><th align='left'>Raw</th><th align='left'>Moisture</th><th align='left'>Age</th></tr>";

  appendSensorRow_(html, soil_.r0(), now);
  appendSensorRow_(html, soil_.r1(), now);
  appendSensorRow_(html, soil_.r2(), now);

  html += "</table>";
  html += "<p><button type='button' onclick='location.reload()'>Refresh</button></p>";
  html += htmlFooter();

  ctx.server.send(200, "text/html", html);
}

static String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); ++i) {
    char c = in[i];
    if (c == '\"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else out += c;
  }
  return out;
}

void SoilMoistureModule::appendSensorJson_(String& json, const SoilMoistureController::Reading& r, uint32_t now) {
  json += "{";
  json += "\"enabled\":" + String(r.enabled ? "true" : "false") + ",";
  json += "\"id\":\"" + jsonEscape(r.id) + "\",";
  json += "\"pin\":" + String(r.pin) + ",";
  json += "\"hasValue\":" + String(r.hasValue ? "true" : "false") + ",";
  json += "\"raw\":" + String(r.hasValue ? r.raw : 0) + ",";
  json += "\"percent\":" + String(r.hasValue ? r.percent : 0) + ",";
  json += "\"ageMs\":" + String(r.hasValue ? (uint32_t)(now - r.lastReadMs) : 0);
  json += "}";
}

void SoilMoistureModule::handleSoilApi_(AppContext& ctx) {
  const uint32_t now = nowMs();

  String json = "{";
  json += "\"intervalMs\":" + String(ctx.config.soil.intervalMs) + ",";
  json += "\"wetRaw\":" + String(ctx.config.soil.wetRaw) + ",";
  json += "\"dryRaw\":" + String(ctx.config.soil.dryRaw) + ",";
  json += "\"sensors\":[";

  appendSensorJson_(json, soil_.r0(), now); json += ",";
  appendSensorJson_(json, soil_.r1(), now); json += ",";
  appendSensorJson_(json, soil_.r2(), now);

  json += "]}";
  ctx.server.send(200, "application/json", json);
}
