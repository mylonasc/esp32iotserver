#include "PumpController.h"

void PumpController::begin(const PumpConfig& cfg) {
  cfg_ = cfg;

  pinMode(cfg_.pinA, OUTPUT);
  pinMode(cfg_.pinB, OUTPUT);
  pinMode(cfg_.pinC, OUTPUT);
  setAllPinsLow_();

  running_ = false;
  activePin_ = 0;
  endMs_ = 0;

  Serial.println("PumpController initialized.");
}

void PumpController::loop() {
  if (!running_) return;

  uint32_t now = millis();
  if ((int32_t)(now - endMs_) >= 0) {
    digitalWrite(activePin_, LOW);
    running_ = false;
    activePin_ = 0;
    endMs_ = 0;
  }
}

bool PumpController::start(PumpId id, int seconds) {
  if (seconds <= 0) return false;
  if (seconds > cfg_.maxSecondsOn) seconds = cfg_.maxSecondsOn;

  if (running_) return false;
  if (!isEnabled_(id)) return false;

  int pin = pinFor_(id);
  if (pin <= 0) return false;

  setAllPinsLow_();
  activePin_ = pin;
  endMs_ = millis() + (uint32_t)seconds * 1000UL;
  running_ = true;

  digitalWrite(activePin_, HIGH);
  return true;
}

bool PumpController::start(char channelLetter, int seconds) {
  bool ok = false;
  PumpId id = fromChar_(channelLetter, ok);
  if (!ok) return false;
  return start(id, seconds);
}

void PumpController::allOff() {
  setAllPinsLow_();
  running_ = false;
  activePin_ = 0;
  endMs_ = 0;
}

float PumpController::remainingSeconds() const {
  if (!running_ || endMs_ == 0) return 0.0f;
  int32_t diff = (int32_t)(endMs_ - millis());
  if (diff <= 0) return 0.0f;
  return diff / 1000.0f;
}

bool PumpController::isEnabled_(PumpId id) const {
  switch (id) {
    case PumpId::A: return cfg_.enabledA;
    case PumpId::B: return cfg_.enabledB;
    case PumpId::C: return cfg_.enabledC;
  }
  return false;
}

int PumpController::pinFor_(PumpId id) const {
  switch (id) {
    case PumpId::A: return cfg_.pinA;
    case PumpId::B: return cfg_.pinB;
    case PumpId::C: return cfg_.pinC;
  }
  return 0;
}

void PumpController::setAllPinsLow_() {
  digitalWrite(cfg_.pinA, LOW);
  digitalWrite(cfg_.pinB, LOW);
  digitalWrite(cfg_.pinC, LOW);
}

PumpController::PumpId PumpController::fromChar_(char c, bool& ok) {
  ok = true;
  if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
  switch (c) {
    case 'A': return PumpId::A;
    case 'B': return PumpId::B;
    case 'C': return PumpId::C;
    default: ok = false; return PumpId::A;
  }
}
