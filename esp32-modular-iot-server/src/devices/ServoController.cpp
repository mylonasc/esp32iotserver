#include "ServoController.h"

void ServoController::begin(const ServoConfig& cfg) {
  stop();

  state_.enabled = cfg.enabled;
  state_.pin = cfg.pin;
  state_.startAngle = normalizeAngle_(cfg.startAngle);
  state_.endAngle = normalizeAngle_(cfg.endAngle);
  state_.motionMode = normalizeMotionMode_(cfg.motionMode);
  state_.holdSeconds = normalizeHoldSeconds_(cfg.holdSeconds);
  state_.stepDelayMs = normalizeStepDelay_(cfg.stepDelayMs);
  state_.id = cfg.id;
  state_.currentAngle = state_.startAngle;
}

void ServoController::loop() {
  if (!state_.running) return;

  const uint32_t now = millis();

  if (phase_ == Phase::Hold) {
    if ((int32_t)(now - holdUntilMs_) >= 0) {
      phase_ = Phase::ToStart;
      lastStepMs_ = now;
    }
    return;
  }

  if ((uint32_t)(now - lastStepMs_) < state_.stepDelayMs) return;
  lastStepMs_ = now;

  int target = phase_ == Phase::ToEnd ? state_.endAngle : state_.startAngle;
  if (state_.currentAngle == target) {
    if (phase_ == Phase::ToEnd && state_.motionMode == 1) {
      phase_ = Phase::ToStart;
      target = state_.startAngle;
    } else if (phase_ == Phase::ToEnd && state_.motionMode == 2) {
      phase_ = Phase::Hold;
      holdUntilMs_ = now + state_.holdSeconds * 1000UL;
      return;
    } else {
      finish_();
      return;
    }
  }

  if (state_.currentAngle < target) ++state_.currentAngle;
  else if (state_.currentAngle > target) --state_.currentAngle;

  servo_.write(state_.currentAngle);
}

bool ServoController::start() {
  if (!state_.enabled || state_.pin <= 0) return false;
  if (state_.running) return false;

  attachIfNeeded_();
  state_.currentAngle = state_.startAngle;
  servo_.write(state_.currentAngle);

  state_.running = true;
  phase_ = Phase::ToEnd;
  lastStepMs_ = millis();
  holdUntilMs_ = 0;

  if (state_.startAngle == state_.endAngle) {
    if (state_.motionMode == 2) {
      phase_ = Phase::Hold;
      holdUntilMs_ = millis() + state_.holdSeconds * 1000UL;
    } else {
      finish_();
    }
  }
  return true;
}

void ServoController::stop() {
  state_.running = false;
  phase_ = Phase::Idle;
  holdUntilMs_ = 0;
  detachIfNeeded_();
}

void ServoController::attachIfNeeded_() {
  if (state_.attached) return;
  servo_.attach(state_.pin);
  state_.attached = true;
}

void ServoController::detachIfNeeded_() {
  if (!state_.attached) return;
  servo_.detach();
  state_.attached = false;
}

void ServoController::finish_() {
  state_.running = false;
  phase_ = Phase::Idle;
  holdUntilMs_ = 0;
  detachIfNeeded_();
}

int ServoController::normalizeAngle_(int angle) {
  if (angle < 0) return 0;
  if (angle > 180) return 180;
  return angle;
}

int ServoController::normalizeMotionMode_(int motionMode) {
  if (motionMode < 0) return 0;
  if (motionMode > 2) return 2;
  return motionMode;
}

uint32_t ServoController::normalizeHoldSeconds_(uint32_t holdSeconds) {
  if (holdSeconds > 3600) return 3600;
  return holdSeconds;
}

uint32_t ServoController::normalizeStepDelay_(uint32_t stepDelayMs) {
  if (stepDelayMs < 1) return 1;
  if (stepDelayMs > 1000) return 1000;
  return stepDelayMs;
}
