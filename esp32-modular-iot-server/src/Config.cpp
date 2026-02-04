#include "Config.h"
#include <Preferences.h>

namespace {
  constexpr const char* NS_CONFIG   = "config";
  constexpr const char* NS_SETTINGS = "settings";
}

AppConfig ConfigStore::load() {
  AppConfig cfg;
  Preferences p;

  // CONFIG namespace (hostname + pumps + soil)
  p.begin(NS_CONFIG, false);

  cfg.hostname = p.getString("hostname", "esp32");

  cfg.pumps.enabledA = p.getBool("pumpA_en", true);
  cfg.pumps.pinA     = p.getInt ("pumpA_pin", 19);

  cfg.pumps.enabledB = p.getBool("pumpB_en", true);
  cfg.pumps.pinB     = p.getInt ("pumpB_pin", 20);

  cfg.pumps.enabledC = p.getBool("pumpC_en", true);
  cfg.pumps.pinC     = p.getInt ("pumpC_pin", 5);

  cfg.pumps.maxSecondsOn = p.getInt("max_sec_on", 10);

  cfg.soil.intervalMs = p.getUInt("soil_int", 300);
  cfg.soil.wetRaw     = p.getInt ("soil_wet", 2300);
  cfg.soil.dryRaw     = p.getInt ("soil_dry", 4095);

  cfg.soil.s0.enabled = p.getBool("soil0_en", false);
  cfg.soil.s0.pin     = p.getInt ("soil0_pin", 35);
  cfg.soil.s0.id      = p.getString("soil0_id", "plant_a");

  cfg.soil.s1.enabled = p.getBool("soil1_en", false);
  cfg.soil.s1.pin     = p.getInt ("soil1_pin", 32);
  cfg.soil.s1.id      = p.getString("soil1_id", "plant_b");

  cfg.soil.s2.enabled = p.getBool("soil2_en", false);
  cfg.soil.s2.pin     = p.getInt ("soil2_pin", 33);
  cfg.soil.s2.id      = p.getString("soil2_id", "plant_c");

  cfg.dht.enabled    = p.getBool("dht_en", false);
  cfg.dht.pin        = p.getInt ("dht_pin", 4);
  cfg.dht.type       = p.getInt ("dht_type", 22);
  cfg.dht.intervalMs = p.getUInt("dht_int", 2000);
  cfg.dht.id         = p.getString("dht_id", "dht");

  cfg.relays.r0.enabled = p.getBool("relay0_en", false);
  cfg.relays.r0.pin     = p.getInt ("relay0_pin", 23);
  cfg.relays.r0.id      = p.getString("relay0_id", "relay_a");
  cfg.relays.r1.enabled = p.getBool("relay1_en", false);
  cfg.relays.r1.pin     = p.getInt ("relay1_pin", 22);
  cfg.relays.r1.id      = p.getString("relay1_id", "relay_b");
  cfg.relays.r2.enabled = p.getBool("relay2_en", false);
  cfg.relays.r2.pin     = p.getInt ("relay2_pin", 21);
  cfg.relays.r2.id      = p.getString("relay2_id", "relay_c");

  p.end();

  p.begin(NS_SETTINGS, false);
  cfg.ssid = p.getString("ssid", "");
  cfg.password = p.getString("password", "");
  p.end();

  Serial.println("Config loaded.");
  return cfg;
}


void ConfigStore::save(const AppConfig& cfg) {
  Preferences p;

  // All "config" keys in one begin/end
  p.begin(NS_CONFIG, false);

  p.putString("hostname", cfg.hostname);

  // pumps
  p.putBool("pumpA_en", cfg.pumps.enabledA);
  p.putInt ("pumpA_pin", cfg.pumps.pinA);
  p.putBool("pumpB_en", cfg.pumps.enabledB);
  p.putInt ("pumpB_pin", cfg.pumps.pinB);
  p.putBool("pumpC_en", cfg.pumps.enabledC);
  p.putInt ("pumpC_pin", cfg.pumps.pinC);
  p.putInt ("max_sec_on", cfg.pumps.maxSecondsOn);

  // soil
  p.putUInt("soil_int", cfg.soil.intervalMs);
  p.putInt ("soil_wet", cfg.soil.wetRaw);
  p.putInt ("soil_dry", cfg.soil.dryRaw);

  p.putBool("soil0_en", cfg.soil.s0.enabled);
  p.putInt ("soil0_pin", cfg.soil.s0.pin);
  p.putString("soil0_id", cfg.soil.s0.id);

  p.putBool("soil1_en", cfg.soil.s1.enabled);
  p.putInt ("soil1_pin", cfg.soil.s1.pin);
  p.putString("soil1_id", cfg.soil.s1.id);

  p.putBool("soil2_en", cfg.soil.s2.enabled);
  p.putInt ("soil2_pin", cfg.soil.s2.pin);
  p.putString("soil2_id", cfg.soil.s2.id);

  p.putBool("dht_en", cfg.dht.enabled);
  p.putInt ("dht_pin", cfg.dht.pin);
  p.putInt ("dht_type", cfg.dht.type);
  p.putUInt("dht_int", cfg.dht.intervalMs);
  p.putString("dht_id", cfg.dht.id);

  p.putBool("relay0_en", cfg.relays.r0.enabled);
  p.putInt ("relay0_pin", cfg.relays.r0.pin);
  p.putString("relay0_id", cfg.relays.r0.id);
  p.putBool("relay1_en", cfg.relays.r1.enabled);
  p.putInt ("relay1_pin", cfg.relays.r1.pin);
  p.putString("relay1_id", cfg.relays.r1.id);
  p.putBool("relay2_en", cfg.relays.r2.enabled);
  p.putInt ("relay2_pin", cfg.relays.r2.pin);
  p.putString("relay2_id", cfg.relays.r2.id);

  p.end();

  // WiFi credentials in "settings"
  p.begin(NS_SETTINGS, false);
  p.putString("ssid", cfg.ssid);
  p.putString("password", cfg.password);
  p.end();

  Serial.println("Config saved.");
}

void ConfigStore::clearWifiCredentials() {
  Preferences p;
  p.begin(NS_SETTINGS, false);
  p.remove("ssid");
  p.remove("password");
  p.end();
  Serial.println("WiFi credentials cleared.");
}
