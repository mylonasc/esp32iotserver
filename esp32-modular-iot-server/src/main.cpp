#include <Arduino.h>

#include "esp_task_wdt.h"
#define CUSTOM_STACK_SIZE 16384

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
// #include "modules/SoilMoistureModule.h"

#include "esp_heap_caps.h"


void loopTask(void *pvParameters);


void validateHeap() {
  size_t total = ESP.getFreeHeap();
  size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  float fragmentation = 100 - (largest * 100.0 / total);
  
  Serial.printf("[MEM] Total Free: %u | Largest Block: %u | Frag: %.2f%%\n", 
                total, largest, fragmentation);
}

static void printHeap(const char* tag) {
  Serial.printf("[%s] free heap: %u, min free heap: %u, largest block: %u\n",
                tag,
                ESP.getFreeHeap(),
                ESP.getMinFreeHeap(),
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}


WebServer server(80);

// Change these from objects to pointers
WifiManager wifi; // Keep this one as a standard object so it can run immediately

// ConfigStore configStore;
// AppConfig config;

// // These are the "heavy" ones to defer
// ModuleManager* modules = nullptr;
// WebUi* ui = nullptr;
// PumpModule* pumpModule = nullptr;
// AppContext* app_ctx = nullptr;

PumpModule pumpModule;
ModuleManager modules;
WebUi ui(modules);
ConfigStore configStore;
AppConfig config;
AppContext app_ctx{ server, configStore, config, wifi };


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
    json += app_ctx.config.hostname;
    json += "\",";

    json += "\"modules\":{";
    modules.appendAllApiStatus(app_ctx, json);
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
    modules.appendAllModuleInfo(app_ctx, json);
    json += "]";
    json += "}";

    server.send(200, "application/json", json);
  });
}

void startServicesOnce() {
  if (servicesStarted) return;
  validateHeap();
  
  Serial.println(F("[MEM] Allocating modules..."));
  
  // Allocate in a specific order
  modules.add(pumpModule);
  

  // CRITICAL: Initialize modules BEFORE registering routes
  modules.beginAll(app_ctx);

  // Register routes
  ui.registerRoutes(app_ctx);
  modules.registerAllRoutes(app_ctx);
  registerCoreApiRoutes();
  Serial.printf("DEBUG: app_ctx: %p, modules: %p, ui: %p\n", (void*)&app_ctx, (void*)&modules, (void*)&ui);
  server.begin();
  servicesStarted = true;
  Serial.println(F("System fully initialized."));
}


void setup() {
  printHeap("boot");
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println("Booting clean modular skeleton...");

  config = configStore.load();
  wifi.begin(config, configStore);
}


void loop() {
  wifi.loop();

  if (wifi.state() == WifiManager::State::PROVISIONING) {
    delay(2);
    return;
  }

  if (wifi.state() == WifiManager::State::CONNECTED) {
    if (!servicesStarted) {
        validateHeap();
        // 3. ONLY NOW run the heavy logic
        modules.add(pumpModule);
        modules.beginAll(app_ctx);
        ui.registerRoutes(app_ctx);
        modules.registerAllRoutes(app_ctx);
        registerCoreApiRoutes();
        
        server.begin();
        servicesStarted = true;
    }
    
    modules.loopAll(app_ctx);
    server.handleClient();
  }
  delay(2);
}


// void loop() {
//   wifi.loop();

//   if (wifi.state() == WifiManager::State::PROVISIONING) {
//     delay(2);
//     return;
//   }

//   if (wifi.state() == WifiManager::State::CONNECTED) {
//     startServicesOnce();
    
//     // Use the arrow operator since they are pointers
//     if (servicesStarted) {
//       modules.loopAll(app_ctx);
//       server.handleClient();
//     }
//   }
//   delay(2);
// }