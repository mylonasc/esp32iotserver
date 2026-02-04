#pragma once
#include <Arduino.h>
#include "Config.h"

struct DhtReading {
  bool enabled = false;
  bool hasValue = false;
  int pin = 0;
  int type = 22;
  float temperatureC = 0.0f;
  float humidity = 0.0f;
  uint32_t lastReadMs = 0;
  String id;
};

class DhtController {
public:
  void begin(const DhtConfig& cfg);
  void loop();

  const DhtReading& reading() const { return reading_; }

private:
  uint32_t lastPollMs_ = 0;
  uint32_t intervalMs_ = 2000;

  DhtReading reading_;

  void readOnce_();
};
