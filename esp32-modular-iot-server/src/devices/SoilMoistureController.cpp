#include "SoilMoistureController.h"

static uint32_t nowMs() { return millis(); }

void SoilMoistureController::begin(const SoilConfig& cfg) {
  cfg_ = cfg;
  applyConfigToReadings_();

  // Configure pins as inputs (ADC pins)
  if (r_[0].enabled) pinMode(r_[0].pin, INPUT);
  if (r_[1].enabled) pinMode(r_[1].pin, INPUT);
  if (r_[2].enabled) pinMode(r_[2].pin, INPUT);

  lastTaskMs_ = 0;
}

void SoilMoistureController::loop() {
  const uint32_t now = nowMs();
  if (cfg_.intervalMs == 0) return;

  if (lastTaskMs_ == 0 || (uint32_t)(now - lastTaskMs_) >= cfg_.intervalMs) {
    lastTaskMs_ = now;
    readOnce_();
  }
}

void SoilMoistureController::applyConfigToReadings_() {
  // Copy config -> runtime metadata
  r_[0].enabled = cfg_.s0.enabled; r_[0].pin = cfg_.s0.pin; r_[0].id = cfg_.s0.id;
  r_[1].enabled = cfg_.s1.enabled; r_[1].pin = cfg_.s1.pin; r_[1].id = cfg_.s1.id;
  r_[2].enabled = cfg_.s2.enabled; r_[2].pin = cfg_.s2.pin; r_[2].id = cfg_.s2.id;

  // Do not reset values unnecessarily; keep last values across reconfig if desired
}

int SoilMoistureController::clampPercent_(int v) {
  if (v < 0) return 0;
  if (v > 100) return 100;
  return v;
}

// Your original: map(raw, 2300, 4095, 100, 0)
// We do equivalent math + clamp and handle weird calibration values.
int SoilMoistureController::mapToPercent_(int raw) const {
  const int wet = cfg_.wetRaw;
  const int dry = cfg_.dryRaw;

  if (dry == wet) return 0;

  // percent = (dry - raw) * 100 / (dry - wet)
  long num = (long)(dry - raw) * 100L;
  long den = (long)(dry - wet);
  long pct = num / den;

  return clampPercent_((int)pct);
}

void SoilMoistureController::readOnce_() {
  const uint32_t now = nowMs();

  for (int i = 0; i < 3; ++i) {
    if (!r_[i].enabled || r_[i].pin < 0) continue;

    int raw = analogRead(r_[i].pin);
    int pct = mapToPercent_(raw);

    r_[i].raw = raw;
    r_[i].percent = pct;
    r_[i].lastReadMs = now;
    r_[i].hasValue = true;
  }
}
