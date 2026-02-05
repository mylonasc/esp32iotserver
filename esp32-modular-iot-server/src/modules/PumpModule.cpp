#include "PumpModule.h"
#include "Html.h"
#include "WebResponse.h"
#include <Arduino.h>
#include <string.h>   // strlcpy (ESP32 has it), memcpy

// ----------------------------
// Local HtmlOut helper (streams to WebServer via sendContent)
// ----------------------------
#include <WebServer.h>

class HtmlOut {
public:
  explicit HtmlOut(WebServer& s) : s_(s) {}

  void begin(const __FlashStringHelper* title) {
    s_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    s_.send(200, "text/html", "");
    s_.sendContent(htmlHeader(title));
  }

  void end() {
    s_.sendContent(htmlFooter());
    s_.client().stop();
  }

  void p(const __FlashStringHelper* t) { s_.sendContent(t); }
  void p(const char* t)                { s_.sendContent(t); }
  void p(const String& t)              { s_.sendContent(t); }

  void pChar(char c) {
    char buf[2] = {c, 0};
    s_.sendContent(buf);
  }

  void pInt(int v) {
    char buf[16];
    itoa(v, buf, 10);
    s_.sendContent(buf);
  }

  void pFloat(float v, int decimals) {
    char buf[24];
    dtostrf(v, 0, decimals, buf);
    char* p = buf;
    while (*p == ' ') ++p;
    s_.sendContent(p);
  }

private:
  WebServer& s_;
};

// ----------------------------
// PumpModule public API
// ----------------------------
void PumpModule::begin(AppContext& ctx) {
  ctx_ = &ctx;
  pumps_.begin(ctx.config.pumps);
}

void PumpModule::loop(AppContext& ctx) {
  (void)ctx;
  const auto& cfg = ctx.config.pumps;
  if (!pumps_.isRunning() && !cfg.enabledA && !cfg.enabledB && !cfg.enabledC) return;
  pumps_.loop();
}

void PumpModule::registerRoutes(AppContext& ctx) {
  ctx_ = &ctx;

  // Capture only `this` (no reference capture to stack variables)
  ctx.server.on("/watering_pumps", HTTP_GET, [this]() {
    handlePumpsPage_(*ctx_);
  });

  ctx.server.on("/api/pumps", HTTP_GET, [this]() {
    handlePumpsApi_(*ctx_);
  });
}

// Print-based fragment: Home tile
void PumpModule::renderHome(AppContext& ctx, Print& out) {
  (void)ctx;

  out.print(F("<div class='box'>"));
  out.print(F("<b>Pumps</b><br>"));

  out.print(F("Running: "));
  out.print(pumps_.isRunning() ? F("Yes") : F("No"));

  out.print(F("<br>Remaining: "));
  // no-heap float formatting:
  char buf[24];
  dtostrf(pumps_.remainingSeconds(), 0, 1, buf);
  char* p = buf; while (*p == ' ') ++p;
  out.print(p);
  out.print(F(" s"));

  out.print(F("<br><a href='/watering_pumps'>Open</a>"));
  out.print(F("</div>"));
}

// Print-based fragment: Config section
void PumpModule::renderConfig(AppContext& ctx, Print& out) {
  auto& p = ctx.config.pumps;

  auto printInt = [&](int v) {
    char b[16];
    itoa(v, b, 10);
    out.print(b);
  };

  out.print(F("<details class='box'>"));
  out.print(F("<summary>Pumps</summary>"));

  // A
  out.print(F("<label><input type='checkbox' name='pumpA_en' "));
  if (p.enabledA) out.print(F("checked"));
  out.print(F("> Enable Pump A</label>"));
  out.print(F("<label>Pump A pin</label>"));
  out.print(F("<input type='number' name='pumpA_pin' value='"));
  printInt(p.pinA);
  out.print(F("'>"));

  // B
  out.print(F("<label><input type='checkbox' name='pumpB_en' "));
  if (p.enabledB) out.print(F("checked"));
  out.print(F("> Enable Pump B</label>"));
  out.print(F("<label>Pump B pin</label>"));
  out.print(F("<input type='number' name='pumpB_pin' value='"));
  printInt(p.pinB);
  out.print(F("'>"));

  // C
  out.print(F("<label><input type='checkbox' name='pumpC_en' "));
  if (p.enabledC) out.print(F("checked"));
  out.print(F("> Enable Pump C</label>"));
  out.print(F("<label>Pump C pin</label>"));
  out.print(F("<input type='number' name='pumpC_pin' value='"));
  printInt(p.pinC);
  out.print(F("'>"));

  // max seconds
  out.print(F("<label>Max seconds on</label>"));
  out.print(F("<input type='number' name='pump_max' min='1' max='600' value='"));
  printInt(p.maxSecondsOn);
  out.print(F("'>"));

  out.print(F("</details>"));
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
    if (v > 600) v = 600; // match UI constraint
    p.maxSecondsOn = v;
  }

  pumps_.begin(p);
}

// Print-based JSON object: {"running":true,...}
void PumpModule::writeApiStatusObject(AppContext& ctx, Print& out) {
  (void)ctx;

  out.print(F("{\"running\":"));
  out.print(pumps_.isRunning() ? F("true") : F("false"));

  out.print(F(",\"activePin\":"));
  out.print(pumps_.activePin());

  out.print(F(",\"remainingSeconds\":"));
  char buf[24];
  dtostrf(pumps_.remainingSeconds(), 0, 2, buf);
  char* p = buf; while (*p == ' ') ++p;
  out.print(p);

  out.print(F("}"));
}

// Print-based JSON object: {"name":"pumps","ui":...}
void PumpModule::writeModuleInfoObject(AppContext& ctx, Print& out) {
  (void)ctx;
  out.print(F("{\"name\":\"pumps\",\"ui\":\"/watering_pumps\",\"api\":\"/api/pumps\"}"));
}

void PumpModule::writeMetrics(AppContext& ctx, Print& out) {
  const auto& cfg = ctx.config.pumps;
  if (!pumps_.isRunning() && !cfg.enabledA && !cfg.enabledB && !cfg.enabledC) return;

  out.print(F("esp32_pumps_running "));
  out.print(pumps_.isRunning() ? F("1") : F("0"));
  out.print('\n');

  out.print(F("esp32_pumps_remaining_seconds "));
  out.print(pumps_.remainingSeconds());
  out.print('\n');
}

void PumpModule::appendMcpTools(Print& out, bool& first) {
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

  addTool(F("pumps.status"),
          F("Get pump runtime status (running, activePin, remainingSeconds)."),
          F("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"));

  addTool(F("pumps.config.get"),
          F("Get current pump configuration (enabled flags, pins, maxSecondsOn)."),
          F("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"));

  addTool(F("pumps.config.set"),
          F("Update pump configuration. Can save/apply and request reboot."),
          F("{\"type\":\"object\",\"properties\":{"
            "\"enabledA\":{\"type\":\"boolean\"},\"enabledB\":{\"type\":\"boolean\"},\"enabledC\":{\"type\":\"boolean\"},"
            "\"pinA\":{\"type\":\"integer\"},\"pinB\":{\"type\":\"integer\"},\"pinC\":{\"type\":\"integer\"},"
            "\"maxSecondsOn\":{\"type\":\"integer\"},"
            "\"save\":{\"type\":\"boolean\"},\"apply\":{\"type\":\"boolean\"},\"reboot\":{\"type\":\"boolean\"}"
          "},\"additionalProperties\":false}"));
}

bool PumpModule::supportsMcpTool(const char* toolName) const {
  return strcmp(toolName, "pumps.status") == 0 ||
         strcmp(toolName, "pumps.config.get") == 0 ||
         strcmp(toolName, "pumps.config.set") == 0;
}

bool PumpModule::handleMcpToolCall(AppContext& ctx,
                                  const char* toolName,
                                  JsonObject args,
                                  Print& out,
                                  bool& rebootRequested) {
  if (strcmp(toolName, "pumps.status") == 0) {
    writeApiStatusObject(ctx, out);
    return true;
  }

  if (strcmp(toolName, "pumps.config.get") == 0) {
    out.print(F("{"));
    out.print(F("\"enabledA\":")); out.print(ctx.config.pumps.enabledA ? F("true") : F("false"));
    out.print(F(",\"enabledB\":")); out.print(ctx.config.pumps.enabledB ? F("true") : F("false"));
    out.print(F(",\"enabledC\":")); out.print(ctx.config.pumps.enabledC ? F("true") : F("false"));
    out.print(F(",\"pinA\":")); out.print(ctx.config.pumps.pinA);
    out.print(F(",\"pinB\":")); out.print(ctx.config.pumps.pinB);
    out.print(F(",\"pinC\":")); out.print(ctx.config.pumps.pinC);
    out.print(F(",\"maxSecondsOn\":")); out.print(ctx.config.pumps.maxSecondsOn);
    out.print(F("}"));
    return true;
  }

  if (strcmp(toolName, "pumps.config.set") == 0) {
    const bool save = !args.containsKey("save") || args["save"].as<bool>();
    const bool apply = !args.containsKey("apply") || args["apply"].as<bool>();
    const bool reboot = args.containsKey("reboot") && args["reboot"].as<bool>();

    if (args.containsKey("enabledA")) ctx.config.pumps.enabledA = args["enabledA"].as<bool>();
    if (args.containsKey("enabledB")) ctx.config.pumps.enabledB = args["enabledB"].as<bool>();
    if (args.containsKey("enabledC")) ctx.config.pumps.enabledC = args["enabledC"].as<bool>();
    if (args.containsKey("pinA")) ctx.config.pumps.pinA = args["pinA"].as<int>();
    if (args.containsKey("pinB")) ctx.config.pumps.pinB = args["pinB"].as<int>();
    if (args.containsKey("pinC")) ctx.config.pumps.pinC = args["pinC"].as<int>();
    if (args.containsKey("maxSecondsOn")) {
      int v = args["maxSecondsOn"].as<int>();
      if (v < 1) v = 1;
      if (v > 600) v = 600;
      ctx.config.pumps.maxSecondsOn = v;
    }

    if (save) ctx.configStore.save(ctx.config);
    if (apply) pumps_.begin(ctx.config.pumps);
    if (reboot) rebootRequested = true;

    out.print(F("{\"saved\":")); out.print(save ? F("true") : F("false"));
    out.print(F(",\"applied\":")); out.print(apply ? F("true") : F("false"));
    out.print(F(",\"rebooting\":")); out.print(reboot ? F("true") : F("false"));
    out.print(F("}"));
    return true;
  }

  return false;
}

// ----------------------------
// Full page handler: streamed, clean helpers
// ----------------------------
static inline char firstCharOrSpace_(const String& s) {
  return s.length() ? s[0] : ' ';
}

PumpModule::PumpActionResult PumpModule::processPumpAction_(AppContext& ctx) {
  auto& s = ctx.server;
  PumpActionResult r;

  if (s.hasArg("off")) {
    pumps_.allOff();
    r.hasMessage = true;
    r.isError = false;
    strlcpy(r.message, "OK: All pumps off.", sizeof(r.message));
    return r;
  }

  if (s.hasArg("ch") && s.hasArg("sec")) {
    const char ch = firstCharOrSpace_(s.arg("ch"));
    int sec = s.arg("sec").toInt();
    sec = constrain(sec, 1, ctx.config.pumps.maxSecondsOn);

    const bool ok = pumps_.start(ch, sec);
    r.hasMessage = true;

    if (ok) {
      r.isError = false;
      snprintf(r.message, sizeof(r.message),
               "OK: Started pump %c for %d seconds.", ch, sec);
    } else {
      r.isError = true;
      strlcpy(r.message,
              "Error: Could not start pump. It may be disabled or already running.",
              sizeof(r.message));
    }
  }

  return r;
}

void PumpModule::renderStatusBox_(HtmlOut& out) {
  out.p(F("<div class='box'>"));

  out.p(F("<b>Running:</b> "));
  out.p(pumps_.isRunning() ? "Yes" : "No");

  out.p(F("<br><b>Active pin:</b> "));
  out.pInt(pumps_.activePin());

  out.p(F("<br><b>Remaining:</b> "));
  out.pFloat(pumps_.remainingSeconds(), 1);
  out.p(F(" s"));

  out.p(F("</div>"));
}

void PumpModule::renderControlForm_(HtmlOut& out, AppContext& ctx) {
  const auto& cfg = ctx.config.pumps;

  out.p(F("<form method='GET' action='/watering_pumps'>"));

  out.p(F("<label>Seconds (max "));
  out.pInt(cfg.maxSecondsOn);
  out.p(F(")</label>"));

  out.p(F("<input type='number' name='sec' min='1' max='"));
  out.pInt(cfg.maxSecondsOn);
  out.p(F("' value='5'>"));

  if (cfg.enabledA) out.p(F("<button name='ch' value='A'>Pump A</button>"));
  if (cfg.enabledB) out.p(F("<button name='ch' value='B'>Pump B</button>"));
  if (cfg.enabledC) out.p(F("<button name='ch' value='C'>Pump C</button>"));

  out.p(F("<button name='off' value='1'>All Off</button>"));
  out.p(F("</form>"));
}

void PumpModule::renderActionMessage_(HtmlOut& out, const PumpActionResult& r) {
  if (!r.hasMessage) return;

  if (r.isError) {
    out.p(F("<p style='color:red;'><b>"));
    out.p(r.message);
    out.p(F("</b></p>"));
  } else {
    out.p(F("<p><b>"));
    out.p(r.message);
    out.p(F("</b></p>"));
  }
}

void PumpModule::handlePumpsPage_(AppContext& ctx) {
  HtmlOut out(ctx.server);

  out.begin(F("Pumps"));
  out.p(F("<h2>Watering Pumps</h2>"));

  renderStatusBox_(out);
  renderControlForm_(out, ctx);

  const PumpActionResult result = processPumpAction_(ctx);
  renderActionMessage_(out, result);

  out.end();
}

// You can keep this as a simple String response, or stream it too.
// Here: stream it (small, no fragmentation).
void PumpModule::handlePumpsApi_(AppContext& ctx) {
  auto res = beginChunkedJson(ctx.server);
  auto& out = res.out();
  out.print(F("{\"pumps\":"));
  writeApiStatusObject(ctx, out);
  out.print(F("}"));
}
