#pragma once
#include <Arduino.h>
#include "Config.h"

class WifiManager {
public:
  enum class State { CONNECTING, CONNECTED, PROVISIONING };

  void begin(AppConfig& cfg, ConfigStore& store);
  void loop();

  State state() const { return state_; }
  bool isConnected() const;

  void resetToProvisioning();

private:
  bool needsInitialization_ = false;
  int connectionAttempts_ = 0;
  int MAX_CONN_ATTEMPTS = 3;
  bool gotProvision_ = false;
  String pendingSsid_;
  String pendingPass_;
  AppConfig* cfg_ = nullptr;
  ConfigStore* store_ = nullptr;

  State state_ = State::PROVISIONING;

  uint32_t lastAttemptMs_ = 0;
  const uint32_t retryIntervalMs_ = 10000;

  void attemptConnect_();
  void startProvisioning_();
};
