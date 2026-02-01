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

// modules must exist before WebUi if WebUi stores a reference/pointer to modules
WebUi ui(modules);

// module instances must be global/static
PumpModule pumpModule;
SoilMoistureModule soilModule;

// ctx must be global/static (and come AFTER its referenced objects exist)
AppContext ctx{ server, configStore, config, wifi };



bool servicesStarted = false;

static void registerCoreApiRoutes() {
  // Combined API endpoint
  server.on("/api", HTTP_GET, []() {
    String json;
    json.reserve(1024);

    json += "{";
    json += "\"uptime_seconds\":";
    json += String(millis() / 1000);
    json += ",";

    json += "\"wifi_status\":\"";
    json += (WiFi.status() == WL_CONNECTED ? "connected" : "not_connected");
    json += "\",";

    json += "\"ip\":\"";
    json += (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String(""));
    json += "\",";

    json += "\"hostname\":\"";
    json += ctx.config.hostname;
    json += "\",";

    json += "\"modules\":{";
    modules.appendAllApiStatus(ctx, json);
    json += "}";

    json += "}";

    server.send(200, "application/json", json);
  });

  // Module registry endpoint
  server.on("/api/modules", HTTP_GET, []() {
    String json;
    json.reserve(512);

    json += "{";
    json += "\"modules\":[";
    modules.appendAllModuleInfo(ctx, json);
    json += "]";
    json += "}";

    server.send(200, "application/json", json);
  });
}

static void startServicesOnce() {
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

  // Core UI routes
  ui.registerRoutes(ctx);

  // Module routes
  modules.registerAllRoutes(ctx);

  // Core API routes (aggregated)
  registerCoreApiRoutes();

  // Avoid WebServer "handler not found" spam (favicon, etc.)
  server.onNotFound([]() {
    if (server.uri() == "/favicon.ico") {
      server.send(204, "image/x-icon", "");
      return;
    }
    server.send(404, "text/plain", "Not found: " + server.uri() + "\n");
  });

  server.begin();
  Serial.println("HTTP server started");
}

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println("Booting clean modular skeleton...");

  config = configStore.load();

  // Install modules
  modules.add(pumpModule);
  modules.add(soilModule);

  // Initialize modules early for pin safety
  modules.beginAll(ctx);

  // Start WiFi with provisioning fallback
  wifi.begin(config, configStore);
}

void loop() {
  wifi.loop();

  // Always tick modules
  modules.loopAll(ctx);

  if (wifi.state() == WifiManager::State::CONNECTED) {
    startServicesOnce();
    server.handleClient();
  }

  delay(2);
}
