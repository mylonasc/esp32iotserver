#pragma once
#include <Arduino.h>
#include "AppContext.h"

class IModule {
public:
  virtual ~IModule() = default;
  virtual const char* name() const = 0;

  // Initialize runtime state (pins, sensors, etc.)
  virtual void begin(AppContext& ctx) = 0;

  // Non-blocking background tick
  virtual void loop(AppContext& ctx) = 0;

  // Register HTTP routes owned by this module
  virtual void registerRoutes(AppContext& ctx) = 0;

  // Optional: contribute fragments to shared pages
  virtual void renderHome(AppContext& ctx, String& html) { (void)ctx; (void)html; }
  virtual void renderConfig(AppContext& ctx, String& html) { (void)ctx; (void)html; }
  virtual void handleConfigPost(AppContext& ctx) { (void)ctx; }
};
