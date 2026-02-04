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
#include "WebResponse.h"
#include "McpServer.h"

// Modules
#include "modules/PumpModule.h"
#include "modules/SoilMoistureModule.h"
#include "modules/DhtModule.h"

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

PumpModule pumpModule;
SoilMoistureModule soilModule;
DhtModule dhtModule;

ModuleManager modules;
WebUi ui(modules);
McpServer mcp;
ConfigStore configStore;
AppConfig config;
AppContext app_ctx{ server, configStore, config, wifi };


bool servicesStarted = false;

static void registerCoreApiRoutes() {
  // Combined API endpoint
  server.on("/api", HTTP_GET, []() {
    auto res = beginChunkedJson(server);
    auto& out = res.out();
    out.print(F("{"));
    out.print(F("\"uptime_seconds\":"));
    out.print((uint32_t)(millis() / 1000));
    out.print(F(","));

    out.print(F("\"wifi_status\":\""));
    out.print(WiFi.status() == WL_CONNECTED ? F("connected") : F("not_connected"));
    out.print(F("\","));

    out.print(F("\"ip\":\""));
    if (WiFi.status() == WL_CONNECTED) {
      out.print(WiFi.localIP().toString());
    }
    out.print(F("\","));

    out.print(F("\"hostname\":\""));
    out.print(app_ctx.config.hostname);
    out.print(F("\","));

    out.print(F("\"modules\":{"));
    modules.writeAllApiStatus(app_ctx, out);
    out.print(F("}"));

    out.print(F("}"));
  });

  // Module registry endpoint
  server.on("/api/modules", HTTP_GET, []() {
    auto res = beginChunkedJson(server);
    auto& out = res.out();
    out.print(F("{"));
    out.print(F("\"modules\":["));
    modules.writeAllModuleInfo(app_ctx, out);
    out.print(F("]"));
    out.print(F("}"));
  });

  // MCP JSON-RPC endpoint
  mcp.registerRoutes(app_ctx, modules);
}

static void startMdns(const String& hostname) {
  if (hostname.length() == 0) return;
  if (!MDNS.begin(hostname.c_str())) {
    Serial.println(F("mDNS failed to start"));
    return;
  }
  MDNS.addService("http", "tcp", 80);
  Serial.print(F("mDNS started: "));
  Serial.print(hostname);
  Serial.println(F(".local"));
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
  modules.add(soilModule);
  modules.add(dhtModule);
        modules.beginAll(app_ctx);
        ui.registerRoutes(app_ctx);
        modules.registerAllRoutes(app_ctx);
        registerCoreApiRoutes();
        startMdns(app_ctx.config.hostname);
        
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
