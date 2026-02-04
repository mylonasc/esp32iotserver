#include "DhtModule.h"
#include "Html.h"
#include "WebResponse.h"

namespace {
  int normalizeType_(int type) {
    if (type == 11 || type == 21 || type == 22) return type;
    return 22;
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

void DhtModule::begin(AppContext& ctx) {
  ctx_ = &ctx;
  dht_.begin(ctx.config.dht);
}

void DhtModule::loop(AppContext& ctx) {
  (void)ctx;
  if (!ctx.config.dht.enabled) return;
  dht_.loop();
}

void DhtModule::registerRoutes(AppContext& ctx) {
  ctx_ = &ctx;
  ctx.server.on("/dht", HTTP_GET, [this]() { handleDhtPage_(*ctx_); });
  ctx.server.on("/api/dht", HTTP_GET, [this]() { handleDhtApi_(*ctx_); });
}

void DhtModule::renderHome(AppContext& ctx, Print& out) {
  const auto& r = dht_.reading();
  const bool running = ctx.config.dht.enabled && ctx.config.dht.intervalMs > 0;

  out.print(F("<div class='box'>"));
  out.print(F("<b>DHT</b><br>"));
  out.print(F("Running: "));
  out.print(running ? F("Yes") : F("No"));
  out.print(F("<br>"));

  if (r.hasValue) {
    out.print(F("Temp: "));
    out.print(r.temperatureC);
    out.print(F(" C<br>Humidity: "));
    out.print(r.humidity);
    out.print(F(" %"));
  } else {
    out.print(F("Temp: —<br>Humidity: —"));
  }

  out.print(F("<br><a href='/dht'>Open</a>"));
  out.print(F("</div>"));
}

void DhtModule::renderConfig(AppContext& ctx, Print& out) {
  auto& c = ctx.config.dht;

  out.print(F("<details class='box'>"));
  out.print(F("<summary>DHT Sensor</summary>"));

  out.print(F("<label><input type='checkbox' name='dht_en' "));
  if (c.enabled) out.print(F("checked"));
  out.print(F("> Enabled</label>"));

  out.print(F("<label>Pin</label>"));
  out.print(F("<input type='number' name='dht_pin' value='"));
  out.print(c.pin);
  out.print(F("'>"));

  out.print(F("<label>Type (11, 21, 22)</label>"));
  out.print(F("<input type='number' name='dht_type' value='"));
  out.print(c.type);
  out.print(F("'>"));

  out.print(F("<label>Read interval (ms)</label>"));
  out.print(F("<input type='number' name='dht_int' min='1000' value='"));
  out.print(c.intervalMs);
  out.print(F("'>"));

  out.print(F("<label>ID</label>"));
  out.print(F("<input name='dht_id' value='"));
  out.print(c.id);
  out.print(F("'>"));

  out.print(F("</details>"));
}

void DhtModule::handleConfigPost(AppContext& ctx) {
  auto& s = ctx.server;
  auto& c = ctx.config.dht;

  c.enabled = s.hasArg("dht_en");
  if (s.hasArg("dht_pin")) c.pin = s.arg("dht_pin").toInt();
  if (s.hasArg("dht_type")) c.type = normalizeType_(s.arg("dht_type").toInt());
  if (s.hasArg("dht_int")) {
    long v = s.arg("dht_int").toInt();
    if (v < 1000) v = 1000;
    c.intervalMs = (uint32_t)v;
  }
  if (s.hasArg("dht_id")) c.id = s.arg("dht_id");

  dht_.begin(c);
}

void DhtModule::writeApiStatusObject(AppContext& ctx, Print& out) {
  const auto& r = dht_.reading();
  const bool running = ctx.config.dht.enabled && ctx.config.dht.intervalMs > 0;

  out.print(F("{\"api\":\"/api/dht\","));
  out.print(F("\"ui\":\"/dht\","));
  out.print(F("\"running\":"));
  out.print(running ? F("true") : F("false"));
  out.print(F(",\"hasValue\":"));
  out.print(r.hasValue ? F("true") : F("false"));
  out.print(F(",\"temperatureC\":"));
  out.print(r.hasValue ? r.temperatureC : 0.0f);
  out.print(F(",\"humidity\":"));
  out.print(r.hasValue ? r.humidity : 0.0f);
  out.print(F("}"));
}

void DhtModule::writeModuleInfoObject(AppContext& ctx, Print& out) {
  (void)ctx;
  out.print(F("{\"name\":\"dht\",\"ui\":\"/dht\",\"api\":\"/api/dht\"}"));
}

void DhtModule::appendMcpTools(Print& out, bool& first) {
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

  addTool(F("dht.readings"),
          F("Get DHT sensor readings and running state."),
          F("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"));

  addTool(F("dht.config.get"),
          F("Get DHT configuration (enabled, pin, type, interval, id)."),
          F("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"));

  addTool(F("dht.config.set"),
          F("Update DHT configuration. Can save/apply and request reboot."),
          F("{\"type\":\"object\",\"properties\":{"
            "\"enabled\":{\"type\":\"boolean\"},\"pin\":{\"type\":\"integer\"},\"type\":{\"type\":\"integer\"},"
            "\"intervalMs\":{\"type\":\"integer\"},\"id\":{\"type\":\"string\"},"
            "\"save\":{\"type\":\"boolean\"},\"apply\":{\"type\":\"boolean\"},\"reboot\":{\"type\":\"boolean\"}"
          "},\"additionalProperties\":false}"));
}

bool DhtModule::supportsMcpTool(const char* toolName) const {
  return strcmp(toolName, "dht.readings") == 0 ||
         strcmp(toolName, "dht.config.get") == 0 ||
         strcmp(toolName, "dht.config.set") == 0;
}

bool DhtModule::handleMcpToolCall(AppContext& ctx,
                                  const char* toolName,
                                  JsonObject args,
                                  Print& out,
                                  bool& rebootRequested) {
  if (strcmp(toolName, "dht.readings") == 0) {
    const auto& r = dht_.reading();
    const auto& cfg = ctx.config.dht;
    const bool running = cfg.enabled && cfg.intervalMs > 0;

    out.print(F("{"));
    out.print(F("\"running\":")); out.print(running ? F("true") : F("false"));
    out.print(F(",\"hasValue\":")); out.print(r.hasValue ? F("true") : F("false"));
    out.print(F(",\"temperatureC\":")); out.print(r.hasValue ? r.temperatureC : 0.0f);
    out.print(F(",\"humidity\":")); out.print(r.hasValue ? r.humidity : 0.0f);
    out.print(F(",\"ageMs\":"));
    out.print(r.hasValue ? (uint32_t)(millis() - r.lastReadMs) : 0);
    out.print(F("}"));
    return true;
  }

  if (strcmp(toolName, "dht.config.get") == 0) {
    const auto& c = ctx.config.dht;
    out.print(F("{"));
    out.print(F("\"enabled\":")); out.print(c.enabled ? F("true") : F("false"));
    out.print(F(",\"pin\":")); out.print(c.pin);
    out.print(F(",\"type\":")); out.print(c.type);
    out.print(F(",\"intervalMs\":")); out.print(c.intervalMs);
    out.print(F(",\"id\":")); printJsonStringQuoted_(out, c.id);
    out.print(F("}"));
    return true;
  }

  if (strcmp(toolName, "dht.config.set") == 0) {
    const bool save = !args.containsKey("save") || args["save"].as<bool>();
    const bool apply = !args.containsKey("apply") || args["apply"].as<bool>();
    const bool reboot = args.containsKey("reboot") && args["reboot"].as<bool>();

    if (args.containsKey("enabled")) ctx.config.dht.enabled = args["enabled"].as<bool>();
    if (args.containsKey("pin")) ctx.config.dht.pin = args["pin"].as<int>();
    if (args.containsKey("type")) ctx.config.dht.type = normalizeType_(args["type"].as<int>());
    if (args.containsKey("intervalMs")) {
      long v = args["intervalMs"].as<long>();
      if (v < 1000) v = 1000;
      ctx.config.dht.intervalMs = (uint32_t)v;
    }
    if (args.containsKey("id")) ctx.config.dht.id = args["id"].as<const char*>();

    if (save) ctx.configStore.save(ctx.config);
    if (apply) dht_.begin(ctx.config.dht);
    if (reboot) rebootRequested = true;

    out.print(F("{\"saved\":")); out.print(save ? F("true") : F("false"));
    out.print(F(",\"applied\":")); out.print(apply ? F("true") : F("false"));
    out.print(F(",\"rebooting\":")); out.print(reboot ? F("true") : F("false"));
    out.print(F("}"));
    return true;
  }

  return false;
}

void DhtModule::handleDhtPage_(AppContext& ctx) {
  const auto& r = dht_.reading();
  const auto& cfg = ctx.config.dht;
  const bool running = cfg.enabled && cfg.intervalMs > 0;

  auto res = beginChunkedHtml(ctx.server, F("DHT"));
  auto& out = res.out();

  out.print(F("<h2>DHT Sensor</h2>"));
  out.print(F("<div class='box'>"));
  out.print(F("<b>Running:</b> "));
  out.print(running ? F("Yes") : F("No"));
  out.print(F("<br><b>Type:</b> "));
  out.print(cfg.type);
  out.print(F("<br><b>Pin:</b> "));
  out.print(cfg.pin);
  out.print(F("<br><b>Interval:</b> "));
  out.print(cfg.intervalMs);
  out.print(F(" ms"));
  out.print(F("<br><b>ID:</b> "));
  out.print(cfg.id);
  out.print(F("</div>"));

  out.print(F("<div class='box'>"));
  if (r.hasValue) {
    out.print(F("<b>Temperature:</b> "));
    out.print(r.temperatureC);
    out.print(F(" C<br><b>Humidity:</b> "));
    out.print(r.humidity);
    out.print(F(" %"));
  } else {
    out.print(F("<b>Temperature:</b> —<br><b>Humidity:</b> —"));
  }
  out.print(F("</div>"));

  out.print(F("<p><a href='/api/dht'>JSON</a>"));
  out.print(F(" <button type='button' onclick='location.reload()'>Refresh</button></p>"));
  out.print(htmlFooter());
}

void DhtModule::handleDhtApi_(AppContext& ctx) {
  const auto& r = dht_.reading();
  const auto& cfg = ctx.config.dht;
  const bool running = cfg.enabled && cfg.intervalMs > 0;

  auto res = beginChunkedJson(ctx.server);
  auto& out = res.out();
  out.print(F("{"));
  out.print(F("\"running\":")); out.print(running ? F("true") : F("false"));
  out.print(F(",\"enabled\":")); out.print(cfg.enabled ? F("true") : F("false"));
  out.print(F(",\"pin\":")); out.print(cfg.pin);
  out.print(F(",\"type\":")); out.print(cfg.type);
  out.print(F(",\"intervalMs\":")); out.print(cfg.intervalMs);
  out.print(F(",\"id\":")); printJsonStringQuoted_(out, cfg.id);
  out.print(F(",\"hasValue\":")); out.print(r.hasValue ? F("true") : F("false"));
  out.print(F(",\"temperatureC\":")); out.print(r.hasValue ? r.temperatureC : 0.0f);
  out.print(F(",\"humidity\":")); out.print(r.hasValue ? r.humidity : 0.0f);
  out.print(F(",\"ageMs\":"));
  out.print(r.hasValue ? (uint32_t)(millis() - r.lastReadMs) : 0);
  out.print(F("}"));
}
