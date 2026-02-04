#include "DhtController.h"
#include <DHT.h>
#include <math.h>

namespace {
  DHT* g_dht = nullptr;

  int normalizeType_(int type) {
    if (type == 11 || type == 21 || type == 22) return type;
    return 22;
  }
}

void DhtController::begin(const DhtConfig& cfg) {
  reading_.enabled = cfg.enabled;
  reading_.pin = cfg.pin;
  reading_.type = normalizeType_(cfg.type);
  reading_.id = cfg.id;
  intervalMs_ = cfg.intervalMs;
  if (intervalMs_ < 1000) intervalMs_ = 1000;

  reading_.hasValue = false;
  reading_.temperatureC = 0.0f;
  reading_.humidity = 0.0f;
  reading_.lastReadMs = 0;
  lastPollMs_ = 0;

  if (g_dht) {
    delete g_dht;
    g_dht = nullptr;
  }

  if (cfg.enabled) {
    g_dht = new DHT(cfg.pin, normalizeType_(cfg.type));
    g_dht->begin();
  }
}

void DhtController::loop() {
  if (!reading_.enabled) return;
  const uint32_t now = millis();
  if (now - lastPollMs_ < intervalMs_) return;
  readOnce_();
  lastPollMs_ = now;
}

void DhtController::readOnce_() {
  if (!g_dht) return;

  const float h = g_dht->readHumidity();
  const float t = g_dht->readTemperature();

  reading_.lastReadMs = millis();
  if (isnan(h) || isnan(t)) {
    reading_.hasValue = false;
    return;
  }

  reading_.hasValue = true;
  reading_.humidity = h;
  reading_.temperatureC = t;
}
