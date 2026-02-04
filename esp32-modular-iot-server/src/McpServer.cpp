#include "McpServer.h"
#include <WiFi.h>
#include "WebResponse.h"

namespace {
  void printJsonString_(Print& out, const String& s) {
    out.print('"');
    for (size_t i = 0; i < s.length(); ++i) {
      char c = s[i];
      if (c == '"') out.print(F("\\\""));
      else if (c == '\\') out.print(F("\\\\"));
      else out.print(c);
    }
    out.print('"');
  }

  void printJsonString_(Print& out, const char* s) {
    if (!s) { out.print(F("\"\"")); return; }
    out.print('"');
    while (*s) {
      char c = *s++;
      if (c == '"') out.print(F("\\\""));
      else if (c == '\\') out.print(F("\\\\"));
      else out.print(c);
    }
    out.print('"');
  }

  void writeJsonRpcError_(WebServer& server, JsonVariantConst id, int code, const char* message) {
    auto res = beginChunkedJson(server);
    auto& out = res.out();
    out.print(F("{\"jsonrpc\":\"2.0\",\"id\":"));
    if (id.isNull()) out.print(F("null"));
    else serializeJson(id, out);
    out.print(F(",\"error\":{\"code\":"));
    out.print(code);
    out.print(F(",\"message\":"));
    printJsonString_(out, message);
    out.print(F("}}"));
  }

  void writeJsonRpcResultStart_(Print& out, JsonVariantConst id) {
    out.print(F("{\"jsonrpc\":\"2.0\",\"id\":"));
    if (id.isNull()) out.print(F("null"));
    else serializeJson(id, out);
    out.print(F(",\"result\":{\"content\":[{\"type\":\"json\",\"json\":"));
  }

  void writeJsonRpcResultEnd_(Print& out) {
    out.print(F("}],\"isError\":false}}"));
  }

  void writeTool_(Print& out,
                  const __FlashStringHelper* name,
                  const __FlashStringHelper* description,
                  const __FlashStringHelper* schema) {
    out.print(F("{\"name\":"));
    out.print('"');
    out.print(name);
    out.print(F("\",\"description\":"));
    out.print('"');
    out.print(description);
    out.print(F("\",\"inputSchema\":"));
    out.print(schema);
    out.print(F("}"));
  }

}

void McpServer::registerRoutes(AppContext& ctx,
                               ModuleManager& modules) {
  ctx.server.on("/mcp", HTTP_POST, [this, &ctx, &modules]() {
    handleMcp_(ctx, modules);
  });
}

void McpServer::handleMcp_(AppContext& ctx,
                           ModuleManager& modules) {
  const String body = ctx.server.arg("plain");
  if (body.isEmpty()) {
    writeJsonRpcError_(ctx.server, JsonVariantConst(), -32700, "Empty request body");
    return;
  }

  DynamicJsonDocument req(4096);
  const DeserializationError err = deserializeJson(req, body);
  if (err) {
    writeJsonRpcError_(ctx.server, JsonVariantConst(), -32700, "Invalid JSON");
    return;
  }

  const char* method = req["method"] | "";
  JsonVariantConst id = req["id"];
  JsonObject params = req["params"].as<JsonObject>();

  if (method[0] == 0) {
    writeJsonRpcError_(ctx.server, id, -32600, "Missing method");
    return;
  }

  if (strcmp(method, "tools/list") == 0) {
    auto res = beginChunkedJson(ctx.server);
    auto& out = res.out();
    out.print(F("{\"jsonrpc\":\"2.0\",\"id\":"));
    if (id.isNull()) out.print(F("null"));
    else serializeJson(id, out);
    out.print(F(",\"result\":{\"tools\":["));

    bool first = true;
    auto addTool = [&](const __FlashStringHelper* name,
                       const __FlashStringHelper* description,
                       const __FlashStringHelper* schema) {
      if (!first) out.print(',');
      first = false;
      writeTool_(out, name, description, schema);
    };

    addTool(F("core.info"),
            F("Get device uptime, WiFi state, IP, and hostname."),
            F("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"));
    addTool(F("modules.list"),
            F("List registered modules with their UI/API metadata."),
            F("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"));
    addTool(F("modules.status"),
            F("Get aggregated runtime status for all modules."),
            F("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"));
    modules.appendMcpTools(out, first);

    out.print(F("]}}"));
    return;
  }

  if (strcmp(method, "tools/call") != 0) {
    writeJsonRpcError_(ctx.server, id, -32601, "Method not found");
    return;
  }

  const char* toolName = params["name"] | "";
  JsonObject args = params["arguments"].as<JsonObject>();
  if (toolName[0] == 0) {
    writeJsonRpcError_(ctx.server, id, -32602, "Missing tool name");
    return;
  }

  if (strcmp(toolName, "core.info") == 0) {
    auto res = beginChunkedJson(ctx.server);
    auto& out = res.out();
    writeJsonRpcResultStart_(out, id);
    out.print(F("{"));
    out.print(F("\"uptime_seconds\":")); out.print((uint32_t)(millis() / 1000));
    out.print(F(",\"wifi_status\":"));
    printJsonString_(out, WiFi.status() == WL_CONNECTED ? "connected" : "not_connected");
    out.print(F(",\"ip\":"));
    if (WiFi.status() == WL_CONNECTED) printJsonString_(out, WiFi.localIP().toString());
    else out.print(F("\"\""));
    out.print(F(",\"hostname\":")); printJsonString_(out, ctx.config.hostname);
    out.print(F("}"));
    writeJsonRpcResultEnd_(out);
    return;
  } else if (strcmp(toolName, "modules.list") == 0) {
    auto res = beginChunkedJson(ctx.server);
    auto& out = res.out();
    writeJsonRpcResultStart_(out, id);
    out.print(F("{\"modules\":["));
    modules.writeAllModuleInfo(ctx, out);
    out.print(F("]}"));
    writeJsonRpcResultEnd_(out);
    return;
  } else if (strcmp(toolName, "modules.status") == 0) {
    auto res = beginChunkedJson(ctx.server);
    auto& out = res.out();
    writeJsonRpcResultStart_(out, id);
    out.print(F("{\"modules\":{"));
    modules.writeAllApiStatus(ctx, out);
    out.print(F("}}"));
    writeJsonRpcResultEnd_(out);
    return;
  }

  if (!modules.supportsMcpTool(toolName)) {
    writeJsonRpcError_(ctx.server, id, -32601, "Unknown tool");
    return;
  }

  bool rebootRequested = false;
  auto res = beginChunkedJson(ctx.server);
  auto& out = res.out();
  writeJsonRpcResultStart_(out, id);
  modules.handleMcpToolCall(ctx, toolName, args, out, rebootRequested);
  writeJsonRpcResultEnd_(out);

  if (rebootRequested) {
    delay(200);
    ESP.restart();
  }
}
