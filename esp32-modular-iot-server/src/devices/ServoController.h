#pragma once
#include <Arduino.h>
#include <ESP32Servo.h>
#include "Config.h"

struct ServoState {
  bool enabled = false;
  bool running = false;
  bool attached = false;
  int pin = 0;
  int startAngle = 0;
  int endAngle = 180;
  int motionMode = 1;
  int currentAngle = 0;
  uint32_t holdSeconds = 2;
  uint32_t stepDelayMs = 15;
  String id;
};

class ServoController {
public:
  void begin(const ServoConfig& cfg);
  void loop();

  bool start();
  void stop();

  const ServoState& state() const { return state_; }

private:
  enum class Phase { Idle, ToEnd, Hold, ToStart };

  Servo servo_;
  ServoState state_;
  Phase phase_ = Phase::Idle;
  uint32_t lastStepMs_ = 0;
  uint32_t holdUntilMs_ = 0;

  void attachIfNeeded_();
  void detachIfNeeded_();
  void finish_();
  static int normalizeAngle_(int angle);
  static int normalizeMotionMode_(int motionMode);
  static uint32_t normalizeHoldSeconds_(uint32_t holdSeconds);
  static uint32_t normalizeStepDelay_(uint32_t stepDelayMs);
};
