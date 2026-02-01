#include "WifiManager.h"
#include <WiFi.h>
#include <WiFiProvisioner.h>

namespace {
  WiFiProvisioner provisioner;
}

void WifiManager::begin(AppConfig& cfg, ConfigStore& store) {
  cfg_ = &cfg;
  store_ = &store;

  WiFi.mode(WIFI_STA);

  if (cfg_->ssid.length() > 0) {
    state_ = State::CONNECTING;
    attemptConnect_();
  } else {
    state_ = State::PROVISIONING;
    startProvisioning_();
  }
}

void WifiManager::loop() {
  if (state_ == State::CONNECTED) return;

  if (state_ == State::CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      state_ = State::CONNECTED;
      Serial.print("WiFi connected. IP: ");
      Serial.println(WiFi.localIP());
      return;
    }

    uint32_t now = millis();
    if (now - lastAttemptMs_ >= retryIntervalMs_) {
      attemptConnect_();
    }
    return;
  }

  // PROVISIONING: WiFiProvisioner runs portal; nothing needed here.
}

bool WifiManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

void WifiManager::attemptConnect_() {
  lastAttemptMs_ = millis();
  Serial.print("Connecting to SSID: ");
  Serial.println(cfg_->ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg_->ssid.c_str(), cfg_->password.c_str());
}

void WifiManager::startProvisioning_() {
  Serial.println("Provisioning mode: connect to AP 'ESP_PROV' and open http://192.168.4.1");

  provisioner.getConfig().SHOW_INPUT_FIELD = false;
  provisioner.getConfig().SHOW_RESET_FIELD = false;

  provisioner.onSuccess([this](const char* ssid, const char* password, const char*) {
    Serial.printf("Provisioned SSID: %s\n", ssid);

    cfg_->ssid = String(ssid);
    cfg_->password = password ? String(password) : String("");

    store_->save(*cfg_);

    state_ = State::CONNECTING;
    attemptConnect_();
  });

  provisioner.startProvisioning();
}

void WifiManager::resetToProvisioning() {
  if (!cfg_ || !store_) return;

  store_->clearWifiCredentials();
  cfg_->ssid = "";
  cfg_->password = "";

  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_STA);

  state_ = State::PROVISIONING;
  startProvisioning_();
}
