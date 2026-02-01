#pragma once
#include <Arduino.h>
#include "Config.h"

class PumpController {
public:
  enum class PumpId { A, B, C };

  void begin(const PumpConfig& cfg);
  void loop();

  bool start(PumpId id, int seconds);
  bool start(char channelLetter, int seconds);

  void allOff();

  bool isRunning() const { return running_; }
  float remainingSeconds() const;
  int activePin() const { return activePin_; }

private:
  PumpConfig cfg_;
  bool running_ = false;
  int activePin_ = 0;
  uint32_t endMs_ = 0;

  bool isEnabled_(PumpId id) const;
  int pinFor_(PumpId id) const;
  void setAllPinsLow_();
  static PumpId fromChar_(char c, bool& ok);
};
