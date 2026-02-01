#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include "Config.h"
#include "WifiManager.h"
#include "AppContext.h"
#include "ModuleManager.h"
#include "WebUi.h"

// Modules
#include "modules/PumpModule.h"
#include "modules/SoilMoistureModule.h"


WebServer server(80);

ConfigStore configStore;
AppConfig config;

WifiManager wifi;
ModuleManager modules;
WebUi ui(modules);

// Install modules here
PumpModule pumpModule;
SoilMoistureModule soilModule;


bool servicesStarted = false;

void startServices(AppContext& ctx) {
  if (servicesStarted) return;
  servicesStarted = true;

  if (config.hostname.length() > 0) {
    if (MDNS.begin(config.hostname.c_str())) {
      Serial.print("mDNS: http://");
      Serial.print(config.hostname);
      Serial.println(".local");
    } else {
      Serial.println("mDNS start failed (non-fatal).");
    }
  }

  // Core routes
  ui.registerRoutes(ctx);

  // Module routes
  modules.registerAllRoutes(ctx);

  server.begin();
  Serial.println("HTTP server started");
}

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println("Booting clean modular skeleton...");

  config = configStore.load();

  // Build context (references)
  AppContext ctx { server, configStore, config, wifi };

  // Install modules
  modules.add(pumpModule);
  modules.add(soilModule);

  // Initialize modules early so pins are safe even before WiFi
  modules.beginAll(ctx);

  // Start WiFi with provisioning fallback
  wifi.begin(config, configStore);
}

void loop() {
  AppContext ctx { server, configStore, config, wifi };

  wifi.loop();

  // Always tick modules (non-blocking)
  modules.loopAll(ctx);

  // Only start/serve HTTP app when connected
  if (wifi.state() == WifiManager::State::CONNECTED) {
    startServices(ctx);
    server.handleClient();
  }

  delay(2);
}
