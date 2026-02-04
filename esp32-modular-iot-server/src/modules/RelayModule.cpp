#include "RelayModule.h"
#include "Html.h"
#include "WebResponse.h"

namespace {
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

void RelayModule::begin(AppContext& ctx) {
  ctx_ = &ctx;
  relays_.begin(ctx.config.relays);
}

void RelayModule::loop(AppContext& ctx) {
  (void)ctx;
}

void RelayModule::registerRoutes(AppContext& ctx) {
  ctx_ = &ctx;
  ctx.server.on("/relays", HTTP_GET, [this]() { handleRelaysPage_(*ctx_); });
  ctx.server.on("/api/relays", HTTP_GET, [this]() { handleRelaysApi_(*ctx_); });
  ctx.server.on("/api/relays/set", HTTP_GET, [this]() { handleRelaySet_(*ctx_); });
}

void RelayModule::renderHome(AppContext& ctx, Print& out) {
  (void)ctx;
  out.print(F("<div class='box'>"));
  out.print(F("<b>Relays</b><br>"));

  for (int i = 0; i < 3; ++i) {
    const auto& r = relays_.state(i);
    if (!r.enabled) continue;
    out.print(r.id);
    out.print(F(": "));
    out.print(r.on ? F("ON") : F("OFF"));
    out.print(F("<br>"));
  }

  out.print(F("<a href='/relays'>Open</a>"));
  out.print(F("</div>"));
}

void RelayModule::renderConfig(AppContext& ctx, Print& out) {
  auto& c = ctx.config.relays;

  auto render = [&](int idx, RelayChannelConfig& rc, const char* label) {
    out.print(F("<hr><b>"));
    out.print(label);
    out.print(F("</b><br>"));
    out.print(F("<label><input type='checkbox' name='relay"));
    out.print(idx);
    out.print(F("_en' "));
    if (rc.enabled) out.print(F("checked"));
    out.print(F("> Enabled</label>"));
    out.print(F("<label>Pin</label>"));
    out.print(F("<input type='number' name='relay"));
    out.print(idx);
    out.print(F("_pin' value='"));
    out.print(rc.pin);
    out.print(F("'>"));
    out.print(F("<label>Name</label>"));
    out.print(F("<input name='relay"));
    out.print(idx);
    out.print(F("_id' value='"));
    out.print(rc.id);
    out.print(F("'>"));
  };

  out.print(F("<details class='box'>"));
  out.print(F("<summary>Relays</summary>"));

  render(0, c.r0, "Relay A");
  render(1, c.r1, "Relay B");
  render(2, c.r2, "Relay C");

  out.print(F("</details>"));
}

void RelayModule::handleConfigPost(AppContext& ctx) {
  auto& s = ctx.server;
  auto& c = ctx.config.relays;

  c.r0.enabled = s.hasArg("relay0_en");
  c.r1.enabled = s.hasArg("relay1_en");
  c.r2.enabled = s.hasArg("relay2_en");

  if (s.hasArg("relay0_pin")) c.r0.pin = s.arg("relay0_pin").toInt();
  if (s.hasArg("relay1_pin")) c.r1.pin = s.arg("relay1_pin").toInt();
  if (s.hasArg("relay2_pin")) c.r2.pin = s.arg("relay2_pin").toInt();

  if (s.hasArg("relay0_id")) c.r0.id = s.arg("relay0_id");
  if (s.hasArg("relay1_id")) c.r1.id = s.arg("relay1_id");
  if (s.hasArg("relay2_id")) c.r2.id = s.arg("relay2_id");

  relays_.begin(c);
}

void RelayModule::writeApiStatusObject(AppContext& ctx, Print& out) {
  (void)ctx;
  out.print(F("{\"api\":\"/api/relays\","));
  out.print(F("\"ui\":\"/relays\","));
  out.print(F("\"states\":["));
  for (int i = 0; i < 3; ++i) {
    const auto& r = relays_.state(i);
    if (i) out.print(',');
    out.print(F("{\"enabled\":")); out.print(r.enabled ? F("true") : F("false"));
    out.print(F(",\"id\":")); printJsonStringQuoted_(out, r.id);
    out.print(F(",\"pin\":")); out.print(r.pin);
    out.print(F(",\"on\":")); out.print(r.on ? F("true") : F("false"));
    out.print(F("}"));
  }
  out.print(F("]}"));
}

void RelayModule::writeModuleInfoObject(AppContext& ctx, Print& out) {
  (void)ctx;
  out.print(F("{\"name\":\"relays\",\"ui\":\"/relays\",\"api\":\"/api/relays\"}"));
}

void RelayModule::writeMetrics(AppContext& ctx, Print& out) {
  (void)ctx;
  for (int i = 0; i < 3; ++i) {
    const auto& r = relays_.state(i);
    if (!r.enabled) continue;
    out.print(F("esp32_relay_state{id=\""));
    printJsonString_(out, r.id);
    out.print(F("\",index=\""));
    out.print(i);
    out.print(F("\"} "));
    out.print(r.on ? F("1") : F("0"));
    out.print('\n');
  }
}

void RelayModule::appendMcpTools(Print& out, bool& first) {
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

  addTool(F("relays.get"),
          F("Get relay states. Optional: index, id, or channel (A/B/C)."),
          F("{\"type\":\"object\",\"properties\":{\"index\":{\"type\":\"integer\"},\"id\":{\"type\":\"string\"},\"channel\":{\"type\":\"string\"}},\"additionalProperties\":false}"));

  addTool(F("relays.set"),
          F("Set relay state by index/id/channel."),
          F("{\"type\":\"object\",\"properties\":{\"index\":{\"type\":\"integer\"},\"id\":{\"type\":\"string\"},\"channel\":{\"type\":\"string\"},\"state\":{\"type\":\"boolean\"}},\"required\":[\"state\"],\"additionalProperties\":false}"));

  addTool(F("relays.config.get"),
          F("Get relay configuration (enabled, pins, ids)."),
          F("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"));

  addTool(F("relays.config.set"),
          F("Update relay configuration. Can save/apply and request reboot."),
          F("{\"type\":\"object\",\"properties\":{"
            "\"r0\":{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"},\"pin\":{\"type\":\"integer\"},\"id\":{\"type\":\"string\"}},\"additionalProperties\":false},"
            "\"r1\":{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"},\"pin\":{\"type\":\"integer\"},\"id\":{\"type\":\"string\"}},\"additionalProperties\":false},"
            "\"r2\":{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"},\"pin\":{\"type\":\"integer\"},\"id\":{\"type\":\"string\"}},\"additionalProperties\":false},"
            "\"save\":{\"type\":\"boolean\"},\"apply\":{\"type\":\"boolean\"},\"reboot\":{\"type\":\"boolean\"}"
          "},\"additionalProperties\":false}"));
}

bool RelayModule::supportsMcpTool(const char* toolName) const {
  return strcmp(toolName, "relays.get") == 0 ||
         strcmp(toolName, "relays.set") == 0 ||
         strcmp(toolName, "relays.config.get") == 0 ||
         strcmp(toolName, "relays.config.set") == 0;
}

int RelayModule::resolveRelayIndex_(const String& token) const {
  if (token.length() == 0) return -1;
  if (token == "A" || token == "a" || token == "0") return 0;
  if (token == "B" || token == "b" || token == "1") return 1;
  if (token == "C" || token == "c" || token == "2") return 2;

  for (int i = 0; i < 3; ++i) {
    if (relays_.state(i).id == token) return i;
  }
  return -1;
}

int RelayModule::resolveRelayIndex_(JsonObject args) const {
  if (args.containsKey("index")) return args["index"].as<int>();
  if (args.containsKey("channel")) return resolveRelayIndex_(args["channel"].as<const char*>());
  if (args.containsKey("id")) return resolveRelayIndex_(args["id"].as<const char*>());
  return -1;
}

bool RelayModule::handleMcpToolCall(AppContext& ctx,
                                    const char* toolName,
                                    JsonObject args,
                                    Print& out,
                                    bool& rebootRequested) {
  (void)ctx;
  if (strcmp(toolName, "relays.get") == 0) {
    int idx = resolveRelayIndex_(args);
    if (idx < 0) {
      out.print(F("{\"relays\":["));
      for (int i = 0; i < 3; ++i) {
        const auto& r = relays_.state(i);
        if (i) out.print(',');
        out.print(F("{\"enabled\":")); out.print(r.enabled ? F("true") : F("false"));
        out.print(F(",\"id\":")); printJsonStringQuoted_(out, r.id);
        out.print(F(",\"pin\":")); out.print(r.pin);
        out.print(F(",\"on\":")); out.print(r.on ? F("true") : F("false"));
        out.print(F("}"));
      }
      out.print(F("]}"));
      return true;
    }

    const auto& r = relays_.state(idx);
    out.print(F("{"));
    out.print(F("\"enabled\":")); out.print(r.enabled ? F("true") : F("false"));
    out.print(F(",\"id\":")); printJsonStringQuoted_(out, r.id);
    out.print(F(",\"pin\":")); out.print(r.pin);
    out.print(F(",\"on\":")); out.print(r.on ? F("true") : F("false"));
    out.print(F("}"));
    return true;
  }

  if (strcmp(toolName, "relays.set") == 0) {
    const bool state = args.containsKey("state") && args["state"].as<bool>();
    int idx = resolveRelayIndex_(args);
    if (idx < 0 || idx > 2) {
      out.print(F("{\"ok\":false,\"error\":\"invalid relay\"}"));
      return true;
    }
    relays_.setState(idx, state);
    out.print(F("{\"ok\":true}"));
    return true;
  }

  if (strcmp(toolName, "relays.config.get") == 0) {
    auto& c = ctx.config.relays;
    out.print(F("{"));
    out.print(F("\"r0\":{"));
    out.print(F("\"enabled\":")); out.print(c.r0.enabled ? F("true") : F("false"));
    out.print(F(",\"pin\":")); out.print(c.r0.pin);
    out.print(F(",\"id\":")); printJsonStringQuoted_(out, c.r0.id);
    out.print(F("},\"r1\":{"));
    out.print(F("\"enabled\":")); out.print(c.r1.enabled ? F("true") : F("false"));
    out.print(F(",\"pin\":")); out.print(c.r1.pin);
    out.print(F(",\"id\":")); printJsonStringQuoted_(out, c.r1.id);
    out.print(F("},\"r2\":{"));
    out.print(F("\"enabled\":")); out.print(c.r2.enabled ? F("true") : F("false"));
    out.print(F(",\"pin\":")); out.print(c.r2.pin);
    out.print(F(",\"id\":")); printJsonStringQuoted_(out, c.r2.id);
    out.print(F("}}"));
    return true;
  }

  if (strcmp(toolName, "relays.config.set") == 0) {
    const bool save = !args.containsKey("save") || args["save"].as<bool>();
    const bool apply = !args.containsKey("apply") || args["apply"].as<bool>();
    const bool reboot = args.containsKey("reboot") && args["reboot"].as<bool>();

    auto& c = ctx.config.relays;
    JsonObject r0 = args["r0"].as<JsonObject>();
    JsonObject r1 = args["r1"].as<JsonObject>();
    JsonObject r2 = args["r2"].as<JsonObject>();

    if (!r0.isNull()) {
      if (r0.containsKey("enabled")) c.r0.enabled = r0["enabled"].as<bool>();
      if (r0.containsKey("pin")) c.r0.pin = r0["pin"].as<int>();
      if (r0.containsKey("id")) c.r0.id = r0["id"].as<const char*>();
    }
    if (!r1.isNull()) {
      if (r1.containsKey("enabled")) c.r1.enabled = r1["enabled"].as<bool>();
      if (r1.containsKey("pin")) c.r1.pin = r1["pin"].as<int>();
      if (r1.containsKey("id")) c.r1.id = r1["id"].as<const char*>();
    }
    if (!r2.isNull()) {
      if (r2.containsKey("enabled")) c.r2.enabled = r2["enabled"].as<bool>();
      if (r2.containsKey("pin")) c.r2.pin = r2["pin"].as<int>();
      if (r2.containsKey("id")) c.r2.id = r2["id"].as<const char*>();
    }

    if (save) ctx.configStore.save(ctx.config);
    if (apply) relays_.begin(c);
    if (reboot) rebootRequested = true;

    out.print(F("{\"saved\":")); out.print(save ? F("true") : F("false"));
    out.print(F(",\"applied\":")); out.print(apply ? F("true") : F("false"));
    out.print(F(",\"rebooting\":")); out.print(reboot ? F("true") : F("false"));
    out.print(F("}"));
    return true;
  }

  return false;
}

void RelayModule::handleRelaysApi_(AppContext& ctx) {
  (void)ctx;
  auto res = beginChunkedJson(ctx.server);
  auto& out = res.out();

  out.print(F("{"));
  out.print(F("\"relays\":["));
  for (int i = 0; i < 3; ++i) {
    const auto& r = relays_.state(i);
    if (i) out.print(',');
    out.print(F("{\"enabled\":")); out.print(r.enabled ? F("true") : F("false"));
    out.print(F(",\"id\":")); printJsonStringQuoted_(out, r.id);
    out.print(F(",\"pin\":")); out.print(r.pin);
    out.print(F(",\"on\":")); out.print(r.on ? F("true") : F("false"));
    out.print(F("}"));
  }
  out.print(F("]}"));
}

void RelayModule::handleRelaySet_(AppContext& ctx) {
  auto& s = ctx.server;
  const String token = s.arg("ch");
  const int idx = resolveRelayIndex_(token);
  if (idx < 0) {
    s.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid channel\"}");
    return;
  }

  const String stateStr = s.arg("state");
  const bool on = (stateStr == "1" || stateStr == "on" || stateStr == "true");
  relays_.setState(idx, on);
  s.send(200, "application/json", "{\"ok\":true}");
}

void RelayModule::handleRelaysPage_(AppContext& ctx) {
  auto res = beginChunkedHtml(ctx.server, F("Relays"));
  auto& out = res.out();

  out.print(F("<h2>Relays</h2>"));

  out.print(F("<div class='box'>"));
  out.print(F("<b>Status</b><br>"));
  for (int i = 0; i < 3; ++i) {
    const auto& r = relays_.state(i);
    if (!r.enabled) continue;
    out.print(r.id);
    out.print(F(": "));
    out.print(r.on ? F("ON") : F("OFF"));
    out.print(F("<br>"));
  }
  out.print(F("</div>"));

  out.print(F("<form method='GET' action='/api/relays/set' class='box'>"));
  out.print(F("<b>Control</b><br>"));
  out.print(F("<label>Channel (A/B/C or id)</label>"));
  out.print(F("<input name='ch' value='A'>"));
  out.print(F("<label>State</label>"));
  out.print(F("<select name='state'><option value='on'>ON</option><option value='off'>OFF</option></select>"));
  out.print(F("<button type='submit'>Set</button>"));
  out.print(F("</form>"));

  out.print(F("<p><a href='/api/relays'>JSON</a>"));
  out.print(F(" <button type='button' onclick='location.reload()'>Refresh</button></p>"));
  out.print(htmlFooter());
}
