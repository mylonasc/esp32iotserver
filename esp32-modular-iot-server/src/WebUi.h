#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "AppContext.h"
#include "ModuleManager.h"

class WebUi {
public:
  explicit WebUi(ModuleManager& modules) : modules_(modules) {}
  void registerRoutes(AppContext& ctx);

private:
  ModuleManager& modules_;
  AppContext* ctx_ = nullptr; // non-owning; must outlive WebUi + route handlers

  void handleRoot_(AppContext& ctx);
  void handleDiagnostics_(AppContext& ctx);
  void handleConfigGet_(AppContext& ctx);
  void handleConfigPost_(AppContext& ctx);
  void handleResetWifi_(AppContext& ctx);
};
