#pragma once
#include <Arduino.h>
#include "Config.h"

struct RelayState {
  bool enabled = false;
  bool on = false;
  int pin = 0;
  String id;
};

class RelayController {
public:
  void begin(const RelaysConfig& cfg);

  void setState(int index, bool on);
  bool getState(int index) const;
  const RelayState& state(int index) const;

private:
  RelayState states_[3];
  void applyPin_(int index);
};
