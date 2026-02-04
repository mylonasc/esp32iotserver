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

    if (cfg_->ssid.length() > 0) {
        state_ = State::CONNECTING;
    } else {
        state_ = State::PROVISIONING;
    }
    // Mark for initialization so loop() handles the heavy lifting
    needsInitialization_ = true; 
}


void WifiManager::loop() {
    // 1. Handle Provisioning Callbacks (Reboot logic)
    if (gotProvision_) {
        gotProvision_ = false;
        cfg_->ssid = pendingSsid_;
        cfg_->password = pendingPass_;
        store_->save(*cfg_);
        Serial.println(F("[PROV] Saved. Rebooting..."));
        delay(300);
        ESP.restart();
    }

    // 2. Handle State Initializations (Prevents Stack Overflow)
    if (needsInitialization_) {
        needsInitialization_ = false;
        if (state_ == State::CONNECTING) {
            attemptConnect_();
        } else if (state_ == State::PROVISIONING) {
            startProvisioning_();
        }
        return; 
    }

    // 3. Normal State Logic
    if (state_ == State::CONNECTED) return;

    if (state_ == State::CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            state_ = State::CONNECTED;
            Serial.print(F("WiFi connected. IP: "));
            Serial.println(WiFi.localIP());
            return;
        }

        if (millis() - lastAttemptMs_ >= retryIntervalMs_) {
            attemptConnect_(); 
        }
    }
    if (state_ == State::CONNECTED) {
      // Optional: Check if we lost connection while running
      if (WiFi.status() != WL_CONNECTED) {
          state_ = State::CONNECTING;
          connectionAttempts_ = 0; 
      }
      return;
    }

    if (state_ == State::CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            state_ = State::CONNECTED;
            connectionAttempts_ = 0; // Reset counter on success
            Serial.println(F("WiFi Connected!"));
            return;
        }

        uint32_t now = millis();
        // Use a 10-second timeout per attempt
        if (now - lastAttemptMs_ >= 10000) { 
            connectionAttempts_++;
            Serial.printf("Connection attempt %d/%d failed.\n", connectionAttempts_, MAX_CONN_ATTEMPTS);
            
            if (connectionAttempts_ >= MAX_CONN_ATTEMPTS) {
                Serial.println(F("Too many failures. Switching to Provisioning Mode..."));
                resetToProvisioning(); 
            } else {
                attemptConnect_(); // Try again
            }
        }
    }
}

void WifiManager::attemptConnect_() {
    lastAttemptMs_ = millis();
    Serial.printf("Connecting to SSID: %s\n", cfg_->ssid.c_str());
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg_->ssid.c_str(), cfg_->password.c_str());
    
}


bool WifiManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

void WifiManager::startProvisioning_() {
  Serial.println(F("--- Entering Provisioning Mode ---"));

  // 1. Clean up any existing WiFi state
  WiFi.disconnect(true, true);
  delay(200); 

  // 2. Set mode to AP only first to reduce power/memory spike
  WiFi.mode(WIFI_AP);
  delay(200);

  // 3. Setup Provisioner Callback
  provisioner.onSuccess([this](const char* ssid, const char* password, const char*) {
    pendingSsid_ = String(ssid);
    pendingPass_ = password ? String(password) : "";
    gotProvision_ = true;
  });

  // 4. Start the provisioner
  // If your library allows, disable its internal "Auto-Scan" if memory is tight
  provisioner.startProvisioning();
  
  Serial.println(F("[INFO] Provisioner logic started. Check 192.168.4.1"));
}

void WifiManager::resetToProvisioning() {
  if (!cfg_ || !store_) return;

  Serial.println(F("Resetting WiFi credentials..."));

  // 1. Clear your application-level config
  store_->clearWifiCredentials();
  cfg_->ssid = "";
  cfg_->password = "";

  // 2. Wipe the ESP32's internal WiFi NVS cache
  WiFi.disconnect(true, true); // (Bool eraseAP, Bool eraseSTA)
  delay(200);

  // 3. Set state so the loop handles the startProvisioning_() safely
  state_ = State::PROVISIONING;
  needsInitialization_ = true; 
}
