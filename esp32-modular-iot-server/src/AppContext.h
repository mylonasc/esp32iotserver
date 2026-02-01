#pragma once
#include <WebServer.h>
#include "Config.h"
#include "WifiManager.h"

struct AppContext {
  WebServer& server;
  ConfigStore& configStore;
  AppConfig& config;
  WifiManager& wifi;
};
