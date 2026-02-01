#pragma once
#include <Arduino.h>
#include "AppContext.h"

class IModule {
public:
  virtual ~IModule() = default;

  // stable module identifier (used in /api aggregation)
  virtual const char* name() const = 0;

  virtual void begin(AppContext& ctx) = 0;
  virtual void loop(AppContext& ctx) = 0;
  virtual void registerRoutes(AppContext& ctx) = 0;

  // Optional: contribute fragments to shared pages
  virtual void renderHome(AppContext& ctx, String& html) { (void)ctx; (void)html; }
  virtual void renderConfig(AppContext& ctx, String& html) { (void)ctx; (void)html; }
  virtual void handleConfigPost(AppContext& ctx) { (void)ctx; }

  // ✅ New: contribute a JSON object for /api under key = module name()
  // Implementations should append a JSON object, e.g. {"running":true,...}
  virtual void appendApiStatusObject(AppContext& ctx, String& json) {
    (void)ctx;
    json += "{}";
  }

  // ✅ New: contribute one entry object to /api/modules array
  // Default just provides name; modules can add ui/api paths.
  virtual void appendModuleInfoObject(AppContext& ctx, String& json) {
    (void)ctx;
    json += "{\"name\":\"";
    json += name();
    json += "\"}";
  }
};
