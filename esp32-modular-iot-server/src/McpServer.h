#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "AppContext.h"
#include "ModuleManager.h"

class McpServer {
public:
  void registerRoutes(AppContext& ctx,
                      ModuleManager& modules);

private:
  void handleMcp_(AppContext& ctx,
                  ModuleManager& modules);
};
