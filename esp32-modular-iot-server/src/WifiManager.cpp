#include "WifiManager.h"
#include <WiFi.h>
#include <WiFiProvisioner.h>

#include "esp_heap_caps.h"

static void printHeap(const char* tag) {
  Serial.printf("[%s] free heap: %u, min free heap: %u, largest block: %u\n",
                tag,
                ESP.getFreeHeap(),
                ESP.getMinFreeHeap(),
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}


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
  // If we just received new creds from portal, handle it safely here
  if (gotProvision_) {
    gotProvision_ = false;

    cfg_->ssid = pendingSsid_;
    cfg_->password = pendingPass_;
    store_->save(*cfg_);

    Serial.println("[PROV] Saved credentials. Rebooting for clean STA connect...");
    delay(300);
    ESP.restart();
  }
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
}
// void WifiManager::loop() {
  // if (state_ == State::CONNECTED) return;

  // if (state_ == State::CONNECTING) {
  //   if (WiFi.status() == WL_CONNECTED) {
  //     state_ = State::CONNECTED;
  //     Serial.print("WiFi connected. IP: ");
  //     Serial.println(WiFi.localIP());
  //     return;
  //   }

  //   uint32_t now = millis();
  //   if (now - lastAttemptMs_ >= retryIntervalMs_) {
  //     attemptConnect_();
  //   }
  //   return;
  // }

//   // PROVISIONING: WiFiProvisioner runs portal; nothing needed here.
// }

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

  // Enable AP + STA so scanning can work reliably
  WiFi.mode(WIFI_AP_STA);

  // Optional but often helps scan stability
  WiFi.setSleep(false);
  delay(100);

  // On newer Arduino-ESP32 cores, STA may need to be explicitly started
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    WiFi.STA.begin();
    delay(50);
  #endif

  // ✅ Diagnostics: heap + WiFi mode
  printHeap("prov_before_start");
  Serial.printf("[DBG] WiFi mode now: %d (1=STA, 2=AP, 3=AP+STA)\n", (int)WiFi.getMode());

  // ✅ Diagnostics: manual scan to see if the ESP32 can actually see SSIDs
  Serial.println("[DBG] Starting manual WiFi.scanNetworks() ...");
  int n = WiFi.scanNetworks(false, true);  // sync scan, include hidden
  Serial.printf("[DBG] scanNetworks found %d networks\n", n);
  for (int i = 0; i < n && i < 10; i++) {
    Serial.printf("  %d: %s (%ddBm) ch %d\n",
                  i, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
  }
  WiFi.scanDelete();
  printHeap("prov_after_manual_scan");

  // TEMPORARY: allow manual entry (useful if UI scan list still empty)
  provisioner.getConfig().SHOW_INPUT_FIELD = true;   // <- set true for debugging
  provisioner.getConfig().SHOW_RESET_FIELD = false;

  provisioner.onSuccess([this](const char* ssid, const char* password, const char*) {
    Serial.printf("[PROV] Received SSID: %s\n", ssid);
    pendingSsid_ = String(ssid);
    pendingPass_ = password ? String(password) : String("");
    gotProvision_ = true;
  });

  // provisioner.onSuccess([this](const char* ssid, const char* password, const char*) {
  //   Serial.printf("Provisioning successful! SSID: %s\n", ssid);

  //   cfg_->ssid = String(ssid);
  //   cfg_->password = password ? String(password) : String("");

  //   store_->save(*cfg_);
  //   state_ = State::CONNECTING;

  //   // Switch to STA for normal operation
  //   WiFi.mode(WIFI_STA);
  //   attemptConnect_();
  // });

  provisioner.startProvisioning();
  Serial.println("[DBG] Provisioner started.");
}


// void WifiManager::startProvisioning_() {
//   Serial.println("Provisioning mode: connect to AP 'ESP_PROV' and open http://192.168.4.1");

//   provisioner.getConfig().SHOW_INPUT_FIELD = false;
//   provisioner.getConfig().SHOW_RESET_FIELD = false;

//   provisioner.onSuccess([this](const char* ssid, const char* password, const char*) {
//     Serial.printf("Provisioned SSID: %s\n", ssid);

//     cfg_->ssid = String(ssid);
//     cfg_->password = password ? String(password) : String("");

//     store_->save(*cfg_);

//     state_ = State::CONNECTING;
//     attemptConnect_();
//   });

//   provisioner.startProvisioning();
// }

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
