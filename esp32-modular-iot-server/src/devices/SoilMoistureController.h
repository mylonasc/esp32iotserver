#pragma once
#include <Arduino.h>
#include "Config.h"

class SoilMoistureController {
public:
  void begin(const SoilConfig& cfg);
  void loop(); // periodic read task

  struct Reading {
    bool enabled = false;
    int pin = -1;
    String id;

    int raw = 0;
    int percent = 0;           // 0..100
    uint32_t lastReadMs = 0;   // millis timestamp
    bool hasValue = false;
  };

  const Reading& r0() const { return r_[0]; }
  const Reading& r1() const { return r_[1]; }
  const Reading& r2() const { return r_[2]; }

  uint32_t intervalMs() const { return cfg_.intervalMs; }
  int wetRaw() const { return cfg_.wetRaw; }
  int dryRaw() const { return cfg_.dryRaw; }

private:
  SoilConfig cfg_;
  Reading r_[3];

  uint32_t lastTaskMs_ = 0;

  static int clampPercent_(int v);
  int mapToPercent_(int raw) const;
  void applyConfigToReadings_();
  void readOnce_();
};
