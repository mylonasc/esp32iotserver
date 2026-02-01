#pragma once
#include <Arduino.h>

struct PumpConfig {
  bool enabledA = true; int pinA = 19;
  bool enabledB = true; int pinB = 20;
  bool enabledC = true; int pinC = 5;
  int maxSecondsOn = 10;
};

struct SoilSensorConfig {
  bool enabled = false;
  int  pin = 35;          // ADC pin
  String id = "plant_a";  // user-defined identifier
};

struct SoilConfig {
  uint32_t intervalMs = 300; // periodic read interval

  // calibration values (same idea as original map(2300..4095 -> 100..0))
  int wetRaw = 2300;
  int dryRaw = 4095;

  // up to 3 sensors
  SoilSensorConfig s0;
  SoilSensorConfig s1;
  SoilSensorConfig s2;
};

struct AppConfig {
  String hostname = "esp32";

  // WiFi creds
  String ssid = "";
  String password = "";

  PumpConfig pumps;

  SoilConfig soil;
};

class ConfigStore {
public:
  AppConfig load();
  void save(const AppConfig& cfg);
  void clearWifiCredentials();
};