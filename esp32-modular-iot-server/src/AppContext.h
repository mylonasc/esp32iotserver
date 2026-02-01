#pragma once
#include <WebServer.h>
#include "Config.h"
#include "WifiManager.h"

enum class RunLevel {
  PROVISIONING,
  CONNECTING,
  CONNECTED
};

struct AppContext {
  WebServer& server;
  ConfigStore& configStore;
  AppConfig& config;
  WifiManager& wifi;

  RunLevel runLevel = RunLevel::PROVISIONING;
};
