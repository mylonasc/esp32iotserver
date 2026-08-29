#include "ServoModule.h"
#include "Html.h"
#include "WebResponse.h"
#include <string.h>

namespace {
  int clampAngle_(int angle) {
    if (angle < 0) return 0;
    if (angle > 180) return 180;
    return angle;
  }

  uint32_t clampStepDelay_(long stepDelayMs) {
    if (stepDelayMs < 1) return 1;
    if (stepDelayMs > 1000) return 1000;
    return (uint32_t)stepDelayMs;
  }

  int clampMotionMode_(int motionMode) {
    if (motionMode < 0) return 0;
    if (motionMode > 2) return 2;
    return motionMode;
  }

  uint32_t clampHoldSeconds_(long holdSeconds) {
    if (holdSeconds < 0) return 0;
    if (holdSeconds > 3600) return 3600;
    return (uint32_t)holdSeconds;
  }

  const __FlashStringHelper* motionModeName_(int motionMode) {
    switch (motionMode) {
      case 0: return F("One-way");
      case 1: return F("Back-and-forth");
      case 2: return F("Move, hold, return");
      default: return F("Unknown");
    }
  }

  void printJsonString_(Print& out, const String& in) {
    for (size_t i = 0; i < in.length(); ++i) {
      char c = in[i];
      if (c == '"') out.print(F("\\\""));
      else if (c == '\\') out.print(F("\\\\"));
      else out.print(c);
    }
  }

  void printJsonStringQuoted_(Print& out, const String& in) {
    out.print('"');
    printJsonString_(out, in);
    out.print('"');
  }
}

void ServoModule::begin(AppContext& ctx) {
  ctx_ = &ctx;
  servo_.begin(ctx.config.servo);
}

void ServoModule::loop(AppContext& ctx) {
  (void)ctx;
  servo_.loop();
}

void ServoModule::registerRoutes(AppContext& ctx) {
  ctx_ = &ctx;
  ctx.server.on("/servo", HTTP_GET, [this]() { handleServoPage_(*ctx_); });
  ctx.server.on("/api/servo", HTTP_GET, [this]() { handleServoApi_(*ctx_); });
  ctx.server.on("/api/servo/start", HTTP_GET, [this]() { handleServoStart_(*ctx_); });
  ctx.server.on("/api/servo/stop", HTTP_GET, [this]() { handleServoStop_(*ctx_); });
}

void ServoModule::renderHome(AppContext& ctx, Print& out) {
  (void)ctx;
  const auto& s = servo_.state();

  out.print(F("<div class='box'>"));
  out.print(F("<b>Servo</b><br>"));
  out.print(F("Enabled: "));
  out.print(s.enabled ? F("Yes") : F("No"));
  out.print(F("<br>Running: "));
  out.print(s.running ? F("Yes") : F("No"));
  out.print(F("<br>Pin: "));
  out.print(s.pin);
  out.print(F("<br>Angle: "));
  out.print(s.currentAngle);
  out.print(F("<br><a href='/servo'>Open</a>"));
  out.print(F("</div>"));
}

void ServoModule::renderConfig(AppContext& ctx, Print& out) {
  auto& c = ctx.config.servo;

  out.print(F("<details class='box'>"));
  out.print(F("<summary>Servo</summary>"));

  out.print(F("<label><input type='checkbox' name='servo_en' "));
  if (c.enabled) out.print(F("checked"));
  out.print(F("> Enabled</label>"));

  out.print(F("<label>Pin</label>"));
  out.print(F("<input type='number' name='servo_pin' value='"));
  out.print(c.pin);
  out.print(F("'>"));

  out.print(F("<label>Start angle</label>"));
  out.print(F("<input type='number' name='servo_start' min='0' max='180' value='"));
  out.print(c.startAngle);
  out.print(F("'>"));

  out.print(F("<label>End angle</label>"));
  out.print(F("<input type='number' name='servo_end' min='0' max='180' value='"));
  out.print(c.endAngle);
  out.print(F("'>"));

  out.print(F("<label>Motion</label>"));
  out.print(F("<select name='servo_mode' id='servo_mode' onchange='servoToggleHold()'>"));
  out.print(F("<option value='0'")); if (c.motionMode == 0) out.print(F(" selected")); out.print(F(">One-way</option>"));
  out.print(F("<option value='1'")); if (c.motionMode == 1) out.print(F(" selected")); out.print(F(">Back-and-forth</option>"));
  out.print(F("<option value='2'")); if (c.motionMode == 2) out.print(F(" selected")); out.print(F(">Move, hold, return</option>"));
  out.print(F("</select>"));

  out.print(F("<div id='servo_hold_box'>"));
  out.print(F("<label>Hold seconds</label>"));
  out.print(F("<input type='number' name='servo_hold' min='0' max='3600' value='"));
  out.print(c.holdSeconds);
  out.print(F("'>"));
  out.print(F("</div>"));

  out.print(F("<label>Step delay (ms)</label>"));
  out.print(F("<input type='number' name='servo_step' min='1' max='1000' value='"));
  out.print(c.stepDelayMs);
  out.print(F("'>"));

  out.print(F("<label>ID</label>"));
  out.print(F("<input name='servo_id' value='"));
  out.print(c.id);
  out.print(F("'>"));

  out.print(F("<script>function servoToggleHold(){var m=document.getElementById('servo_mode');var b=document.getElementById('servo_hold_box');if(m&&b)b.style.display=m.value==='2'?'block':'none';}servoToggleHold();</script>"));

  out.print(F("</details>"));
}

void ServoModule::handleConfigPost(AppContext& ctx) {
  auto& s = ctx.server;
  auto& c = ctx.config.servo;

  c.enabled = s.hasArg("servo_en");
  if (s.hasArg("servo_pin")) c.pin = s.arg("servo_pin").toInt();
  if (s.hasArg("servo_start")) c.startAngle = clampAngle_(s.arg("servo_start").toInt());
  if (s.hasArg("servo_end")) c.endAngle = clampAngle_(s.arg("servo_end").toInt());
  if (s.hasArg("servo_mode")) c.motionMode = clampMotionMode_(s.arg("servo_mode").toInt());
  if (s.hasArg("servo_hold")) c.holdSeconds = clampHoldSeconds_(s.arg("servo_hold").toInt());
  if (s.hasArg("servo_step")) c.stepDelayMs = clampStepDelay_(s.arg("servo_step").toInt());
  if (s.hasArg("servo_id")) c.id = s.arg("servo_id");

  servo_.begin(c);
}

void ServoModule::writeApiStatusObject(AppContext& ctx, Print& out) {
  (void)ctx;
  const auto& s = servo_.state();

  out.print(F("{\"api\":\"/api/servo\","));
  out.print(F("\"ui\":\"/servo\","));
  out.print(F("\"enabled\":")); out.print(s.enabled ? F("true") : F("false"));
  out.print(F(",\"id\":")); printJsonStringQuoted_(out, s.id);
  out.print(F(",\"pin\":")); out.print(s.pin);
  out.print(F(",\"startAngle\":")); out.print(s.startAngle);
  out.print(F(",\"endAngle\":")); out.print(s.endAngle);
  out.print(F(",\"motionMode\":")); out.print(s.motionMode);
  out.print(F(",\"motionModeName\":\"")); out.print(motionModeName_(s.motionMode)); out.print(F("\""));
  out.print(F(",\"holdSeconds\":")); out.print(s.holdSeconds);
  out.print(F(",\"stepDelayMs\":")); out.print(s.stepDelayMs);
  out.print(F(",\"running\":")); out.print(s.running ? F("true") : F("false"));
  out.print(F(",\"attached\":")); out.print(s.attached ? F("true") : F("false"));
  out.print(F(",\"currentAngle\":")); out.print(s.currentAngle);
  out.print(F("}"));
}

void ServoModule::writeModuleInfoObject(AppContext& ctx, Print& out) {
  (void)ctx;
  out.print(F("{\"name\":\"servo\",\"ui\":\"/servo\",\"api\":\"/api/servo\"}"));
}

void ServoModule::writeMetrics(AppContext& ctx, Print& out) {
  (void)ctx;
  const auto& s = servo_.state();
  if (!s.enabled) return;

  out.print(F("esp32_servo_running{id=\""));
  printJsonString_(out, s.id);
  out.print(F("\"} "));
  out.print(s.running ? F("1") : F("0"));
  out.print('\n');

  out.print(F("esp32_servo_current_angle{id=\""));
  printJsonString_(out, s.id);
  out.print(F("\"} "));
  out.print(s.currentAngle);
  out.print('\n');
}

void ServoModule::appendMcpTools(Print& out, bool& first) {
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

  addTool(F("servo.status"),
          F("Get servo state and configuration."),
          F("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"));

  addTool(F("servo.start"),
          F("Start the configured servo movement."),
          F("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"));

  addTool(F("servo.stop"),
          F("Stop servo movement and detach the PWM signal."),
          F("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"));

  addTool(F("servo.config.get"),
          F("Get servo configuration."),
          F("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"));

  addTool(F("servo.config.set"),
          F("Update servo configuration. Can save/apply and request reboot."),
          F("{\"type\":\"object\",\"properties\":{"
            "\"enabled\":{\"type\":\"boolean\"},\"pin\":{\"type\":\"integer\"},"
            "\"startAngle\":{\"type\":\"integer\"},\"endAngle\":{\"type\":\"integer\"},"
            "\"motionMode\":{\"type\":\"integer\"},\"holdSeconds\":{\"type\":\"integer\"},"
            "\"stepDelayMs\":{\"type\":\"integer\"},"
            "\"id\":{\"type\":\"string\"},\"save\":{\"type\":\"boolean\"},"
            "\"apply\":{\"type\":\"boolean\"},\"reboot\":{\"type\":\"boolean\"}"
          "},\"additionalProperties\":false}"));
}

bool ServoModule::supportsMcpTool(const char* toolName) const {
  return strcmp(toolName, "servo.status") == 0 ||
         strcmp(toolName, "servo.start") == 0 ||
         strcmp(toolName, "servo.stop") == 0 ||
         strcmp(toolName, "servo.config.get") == 0 ||
         strcmp(toolName, "servo.config.set") == 0;
}

bool ServoModule::handleMcpToolCall(AppContext& ctx,
                                   const char* toolName,
                                   JsonObject args,
                                   Print& out,
                                   bool& rebootRequested) {
  if (strcmp(toolName, "servo.status") == 0) {
    writeApiStatusObject(ctx, out);
    return true;
  }

  if (strcmp(toolName, "servo.start") == 0) {
    const bool ok = servo_.start();
    out.print(F("{\"ok\":")); out.print(ok ? F("true") : F("false"));
    out.print(F(",\"error\":"));
    if (ok) out.print(F("null"));
    else out.print(F("\"servo disabled, invalid, or already running\""));
    out.print(F("}"));
    return true;
  }

  if (strcmp(toolName, "servo.stop") == 0) {
    servo_.stop();
    out.print(F("{\"ok\":true}"));
    return true;
  }

  if (strcmp(toolName, "servo.config.get") == 0) {
    printConfigObject_(ctx.config.servo, out);
    return true;
  }

  if (strcmp(toolName, "servo.config.set") == 0) {
    const bool save = !args.containsKey("save") || args["save"].as<bool>();
    const bool apply = !args.containsKey("apply") || args["apply"].as<bool>();
    const bool reboot = args.containsKey("reboot") && args["reboot"].as<bool>();

    auto& c = ctx.config.servo;
    if (args.containsKey("enabled")) c.enabled = args["enabled"].as<bool>();
    if (args.containsKey("pin")) c.pin = args["pin"].as<int>();
    if (args.containsKey("startAngle")) c.startAngle = clampAngle_(args["startAngle"].as<int>());
    if (args.containsKey("endAngle")) c.endAngle = clampAngle_(args["endAngle"].as<int>());
    if (args.containsKey("motionMode")) c.motionMode = clampMotionMode_(args["motionMode"].as<int>());
    if (args.containsKey("holdSeconds")) c.holdSeconds = clampHoldSeconds_(args["holdSeconds"].as<long>());
    if (args.containsKey("stepDelayMs")) c.stepDelayMs = clampStepDelay_(args["stepDelayMs"].as<long>());
    if (args.containsKey("id")) c.id = args["id"].as<const char*>();

    if (save) ctx.configStore.save(ctx.config);
    if (apply) servo_.begin(c);
    if (reboot) rebootRequested = true;

    out.print(F("{\"saved\":")); out.print(save ? F("true") : F("false"));
    out.print(F(",\"applied\":")); out.print(apply ? F("true") : F("false"));
    out.print(F(",\"rebooting\":")); out.print(reboot ? F("true") : F("false"));
    out.print(F("}"));
    return true;
  }

  return false;
}

void ServoModule::handleServoPage_(AppContext& ctx) {
  const auto& s = servo_.state();
  auto res = beginChunkedHtml(ctx.server, F("Servo"));
  auto& out = res.out();

  out.print(F("<h2>Servo</h2>"));
  out.print(F("<div class='box'>"));
  out.print(F("<b>Enabled:</b> ")); out.print(s.enabled ? F("Yes") : F("No"));
  out.print(F("<br><b>Running:</b> ")); out.print(s.running ? F("Yes") : F("No"));
  out.print(F("<br><b>Attached:</b> ")); out.print(s.attached ? F("Yes") : F("No"));
  out.print(F("<br><b>Pin:</b> ")); out.print(s.pin);
  out.print(F("<br><b>Start angle:</b> ")); out.print(s.startAngle);
  out.print(F("<br><b>End angle:</b> ")); out.print(s.endAngle);
  out.print(F("<br><b>Current angle:</b> ")); out.print(s.currentAngle);
  out.print(F("<br><b>Mode:</b> ")); out.print(motionModeName_(s.motionMode));
  if (s.motionMode == 2) {
    out.print(F("<br><b>Hold:</b> ")); out.print(s.holdSeconds); out.print(F(" s"));
  }
  out.print(F("<br><b>Step delay:</b> ")); out.print(s.stepDelayMs); out.print(F(" ms"));
  out.print(F("<br><b>ID:</b> ")); out.print(s.id);
  out.print(F("</div>"));

  out.print(F("<div class='box'>"));
  out.print(F("<button type='button' onclick=\"servoCmd('/api/servo/start')\">Start Movement</button>"));
  out.print(F("<button type='button' onclick=\"servoCmd('/api/servo/stop')\">Stop And Detach</button>"));
  out.print(F("<div id='servo_msg'></div>"));
  out.print(F("</div>"));

  out.print(F("<p><a href='/api/servo'>JSON</a>"));
  out.print(F(" <button type='button' onclick='location.reload()'>Refresh</button></p>"));
  out.print(F("<script>function servoCmd(u){var m=document.getElementById('servo_msg');fetch(u).then(function(r){return r.json().then(function(j){return{ok:r.ok,j:j};});}).then(function(x){m.textContent=x.ok?'OK':('Error: '+(x.j.error||'request failed'));}).catch(function(e){m.textContent='Error: '+e;});}</script>"));
  out.print(htmlFooter());
}

void ServoModule::handleServoApi_(AppContext& ctx) {
  auto res = beginChunkedJson(ctx.server);
  writeApiStatusObject(ctx, res.out());
}

void ServoModule::handleServoStart_(AppContext& ctx) {
  const bool ok = servo_.start();
  if (!ok) {
    ctx.server.send(409, "application/json", "{\"ok\":false,\"error\":\"servo disabled, invalid, or already running\"}");
    return;
  }
  ctx.server.send(200, "application/json", "{\"ok\":true}");
}

void ServoModule::handleServoStop_(AppContext& ctx) {
  servo_.stop();
  ctx.server.send(200, "application/json", "{\"ok\":true}");
}

void ServoModule::printConfigObject_(const ServoConfig& c, Print& out) const {
  out.print(F("{"));
  out.print(F("\"enabled\":")); out.print(c.enabled ? F("true") : F("false"));
  out.print(F(",\"pin\":")); out.print(c.pin);
  out.print(F(",\"startAngle\":")); out.print(c.startAngle);
  out.print(F(",\"endAngle\":")); out.print(c.endAngle);
  out.print(F(",\"motionMode\":")); out.print(c.motionMode);
  out.print(F(",\"motionModeName\":\"")); out.print(motionModeName_(c.motionMode)); out.print(F("\""));
  out.print(F(",\"holdSeconds\":")); out.print(c.holdSeconds);
  out.print(F(",\"stepDelayMs\":")); out.print(c.stepDelayMs);
  out.print(F(",\"id\":")); printJsonStringQuoted_(out, c.id);
  out.print(F("}"));
}
