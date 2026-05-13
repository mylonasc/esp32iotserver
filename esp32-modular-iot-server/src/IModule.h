#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "AppContext.h"

class IModule {
public:
  virtual ~IModule() = default;

  // stable module identifier (used in /api aggregation)
  virtual const char* name() const = 0;

  virtual void begin(AppContext& ctx) = 0;
  virtual void loop(AppContext& ctx) = 0;
  virtual void registerRoutes(AppContext& ctx) = 0;

  // --------------------------------------------------------------------------
  // New preferred API: stream to Print sinks (low fragmentation).
  //
  // Implement these in new/updated modules.
  // The caller (aggregator) decides whether Print is a network client,
  // a response stream, or even a StringPrint wrapper.
  // --------------------------------------------------------------------------

  // Optional: contribute fragments to shared pages
  virtual void renderHome(AppContext& ctx, Print& out)    { (void)ctx; (void)out; }
  virtual void renderConfig(AppContext& ctx, Print& out)  { (void)ctx; (void)out; }
  virtual void handleConfigPost(AppContext& ctx)          { (void)ctx; }

  // Contribute a JSON object for /api under key = module name().
  // Implementations should print a JSON object only: {"running":true,...}
  virtual void writeApiStatusObject(AppContext& ctx, Print& out) {
    (void)ctx;
    out.print(F("{}"));
  }

  // Contribute metrics in Prometheus text format (no surrounding header).
  // Modules should only emit lines for enabled/active devices.
  virtual void writeMetrics(AppContext& ctx, Print& out) {
    (void)ctx;
    (void)out;
  }

  // Contribute one entry object to /api/modules array.
  // Default provides {"name":"..."}; modules can add ui/api paths, etc.
  virtual void writeModuleInfoObject(AppContext& ctx, Print& out) {
    (void)ctx;
    out.print(F("{\"name\":\""));
    out.print(name());
    out.print(F("\"}"));
  }

  // --------------------------------------------------------------------------
  // MCP tools (optional per-module)
  // --------------------------------------------------------------------------

  // Append MCP tool definitions (JSON objects) to tools list.
  // Use `first` to manage commas in an array.
  virtual void appendMcpTools(Print& out, bool& first) {
    (void)out;
    (void)first;
  }

  virtual bool supportsMcpTool(const char* toolName) const {
    (void)toolName;
    return false;
  }

  // Handle an MCP tool call for this module.
  // If handled, write the JSON result object to `out` and return true.
  // Set `rebootRequested` if the tool wants a reboot after responding.
  virtual bool handleMcpToolCall(AppContext& ctx,
                                 const char* toolName,
                                 JsonObject args,
                                 Print& out,
                                 bool& rebootRequested) {
    (void)ctx;
    (void)toolName;
    (void)args;
    (void)out;
    (void)rebootRequested;
    return false;
  }
};
