#include "RelayController.h"

void RelayController::begin(const RelaysConfig& cfg) {
  states_[0].enabled = cfg.r0.enabled;
  states_[0].pin = cfg.r0.pin;
  states_[0].id = cfg.r0.id;
  states_[0].on = false;

  states_[1].enabled = cfg.r1.enabled;
  states_[1].pin = cfg.r1.pin;
  states_[1].id = cfg.r1.id;
  states_[1].on = false;

  states_[2].enabled = cfg.r2.enabled;
  states_[2].pin = cfg.r2.pin;
  states_[2].id = cfg.r2.id;
  states_[2].on = false;

  for (int i = 0; i < 3; ++i) {
    if (states_[i].enabled) {
      pinMode(states_[i].pin, OUTPUT);
      digitalWrite(states_[i].pin, LOW);
    }
  }
}

void RelayController::applyPin_(int index) {
  if (index < 0 || index > 2) return;
  if (!states_[index].enabled) return;
  digitalWrite(states_[index].pin, states_[index].on ? HIGH : LOW);
}

void RelayController::setState(int index, bool on) {
  if (index < 0 || index > 2) return;
  if (!states_[index].enabled) return;
  states_[index].on = on;
  applyPin_(index);
}

bool RelayController::getState(int index) const {
  if (index < 0 || index > 2) return false;
  return states_[index].on;
}

const RelayState& RelayController::state(int index) const {
  return states_[index < 0 ? 0 : (index > 2 ? 2 : index)];
}
