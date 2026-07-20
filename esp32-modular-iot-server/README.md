# ESP32 Modular IoT Server

This is a PlatformIO/Arduino firmware for an ESP32 that exposes local IoT devices through a browser UI, JSON APIs, Prometheus-style metrics, and an MCP JSON-RPC endpoint for automation/agents.

The firmware is organized around a device + module architecture:

- `src/devices/`: low-level hardware controllers for pins, sampling, and device state.
- `src/modules/`: integrations that own devices and expose UI, HTTP routes, config handling, metrics, and MCP tools.
- `src/main.cpp`: application entry point, WiFi/service startup, core API routes, metrics, and module registration.
- `apps/tools/`: Python tooling for scraping ESP32 devices and exporting OpenTelemetry metrics.
- `apps/agents/`: LangGraph/LLM helpers for working with the ESP32 MCP endpoint.
- `test/agents/`: live-device smoke tests and sample MCP agent scripts.

The app is intended for trusted local-network use. HTTP, JSON API, metrics, and MCP endpoints do not implement authentication.

## Current Modules

| Module | Purpose | UI | JSON API | Notes |
| --- | --- | --- | --- | --- |
| Pumps | Controls up to 3 watering pump outputs | `/watering_pumps` | `/api/pumps` | Supports timed pump runs and all-off control. |
| Soil moisture | Reads up to 3 analog soil sensors | `/soil` | `/api/soil` | Supports calibration and per-sensor IDs. |
| DHT | Reads one DHT11/DHT21/DHT22 temperature/humidity sensor | `/dht` | `/api/dht` | Sensor type and interval are configurable. |
| Relays | Controls up to 3 relay outputs | `/relays` | `/api/relays` | Also exposes `/api/relays/set` for simple state changes. |

## Core Routes

| Route | Method | Description |
| --- | --- | --- |
| `/` | GET | Home dashboard with WiFi status and module summaries. |
| `/config` | GET | Configuration page for network and module settings. |
| `/config` | POST | Saves configuration. Supports live apply or save-and-reboot. |
| `/diag` | GET | Runtime diagnostics such as heap, flash, uptime, and task count. |
| `/reset_wifi` | POST | Clears saved WiFi credentials and reboots into provisioning mode. |
| `/api` | GET | Aggregate JSON status, including core state and module summaries. |
| `/api/modules` | GET | Registered module metadata. |
| `/metrics` | GET | Prometheus-compatible text metrics emitted by active modules. |
| `/mcp` | POST | MCP-style JSON-RPC endpoint for automation tools. |

## Boot Flow

1. `ConfigStore` loads `AppConfig` from ESP32 Preferences.
2. `WifiManager` starts WiFi connection if credentials exist, otherwise enters provisioning mode.
3. While provisioning, the web server and modules are not started.
4. After WiFi reaches `CONNECTED`, `main.cpp` registers modules, initializes them, registers routes, starts mDNS, and starts the web server.
5. `loop()` keeps WiFi state updated, ticks each module, and calls `server.handleClient()`.

mDNS is started with the configured hostname, so a device with hostname `esp32-plants` should be reachable as `http://esp32-plants.local/` on networks that support mDNS.

## Configuration

Configuration is persisted in ESP32 Preferences:

- General device/module settings are stored in the `config` namespace.
- WiFi credentials are stored in the `settings` namespace.

The `/config` page saves all module config after each module parses its submitted fields. `Save (Apply)` applies changes without a reboot where possible. `Save & Reboot` persists settings and restarts the ESP32.

Pin and value validation exists for some fields, but it is not yet centralized across every HTTP and MCP config path.

## MCP Tools

The server exposes an MCP-style JSON-RPC endpoint at:

- `POST /mcp`

Supported JSON-RPC methods:

- `tools/list`: lists available tools.
- `tools/call`: invokes one tool by name.

Implemented tools:

| Tool | Description |
| --- | --- |
| `core.info` | Device uptime, WiFi state, IP, and hostname. |
| `modules.list` | Registered module metadata. |
| `modules.status` | Aggregated module status. |
| `pumps.status` | Pump runtime state. |
| `pumps.config.get` | Current pump configuration. |
| `pumps.config.set` | Update pump configuration and optionally save/apply/reboot. |
| `soil.readings` | Soil sensor readings and calibration state. |
| `soil.config.get` | Current soil configuration. |
| `soil.config.set` | Update soil configuration and optionally save/apply/reboot. |
| `dht.readings` | DHT reading and runtime state. |
| `dht.config.get` | Current DHT configuration. |
| `dht.config.set` | Update DHT configuration and optionally save/apply/reboot. |
| `relays.get` | Relay state by index, channel, ID, or all relays. |
| `relays.set` | Set relay state by index, channel, or ID. |
| `relays.config.get` | Current relay configuration. |
| `relays.config.set` | Update relay configuration and optionally save/apply/reboot. |

## Metrics

The firmware exposes Prometheus-compatible metrics at:

- `GET /metrics`

Each module contributes metrics via `IModule::writeMetrics(AppContext&, Print&)`. Disabled devices are generally omitted.

Examples include:

- `esp32_pumps_running`
- `esp32_pumps_remaining_seconds`
- `esp32_soil_moisture_percent{id="..."}`
- `esp32_soil_raw{id="..."}`
- `esp32_dht_temp_c{id="..."}`
- `esp32_dht_humidity_percent{id="..."}`
- `esp32_relay_state{id="...",index="..."}`

## Building And Flashing

Build from this directory:

```bash
pio run
```

Upload to a connected ESP32:

```bash
pio run --target upload
```

Open the serial monitor:

```bash
pio device monitor
```

The PlatformIO environment is defined in `platformio.ini` and currently targets `esp32dev` with the Arduino framework.

## Adding A Module

Add reusable hardware logic under `src/devices/` and expose it through a module under `src/modules/`.

A module implements `IModule`:

```cpp
class IModule {
public:
  virtual const char* name() const = 0;
  virtual void begin(AppContext& ctx) = 0;
  virtual void loop(AppContext& ctx) = 0;
  virtual void registerRoutes(AppContext& ctx) = 0;

  virtual void renderHome(AppContext& ctx, Print& out);
  virtual void renderConfig(AppContext& ctx, Print& out);
  virtual void handleConfigPost(AppContext& ctx);
  virtual void writeApiStatusObject(AppContext& ctx, Print& out);
  virtual void writeMetrics(AppContext& ctx, Print& out);
  virtual void appendMcpTools(Print& out, bool& first);
  virtual bool supportsMcpTool(const char* toolName) const;
  virtual bool handleMcpToolCall(AppContext& ctx,
                                 const char* toolName,
                                 JsonObject args,
                                 Print& out,
                                 bool& rebootRequested);
};
```

Typical steps:

1. Add config fields in `src/Config.h`.
2. Load and save those fields in `src/Config.cpp`.
3. Add a controller in `src/devices/` if the hardware behavior is reusable.
4. Add a module in `src/modules/` that owns the controller.
5. Register module routes in `registerRoutes()`.
6. Contribute fragments to the home/config pages with `Print&` streaming.
7. Add API, metrics, and MCP support where useful.
8. Include and add the module in `src/main.cpp`.

Keep `loop()` non-blocking. Prefer `millis()` timers over `delay()` inside module logic.

## Python Tooling

Python code lives under `apps/`:

- `apps/tools/esp32_scraper.py`: discovers module API endpoints, scrapes readings/state, and exports metrics through OTLP.
- `apps/tools/run_scrape.py`: example fleet scrape/export script with hardcoded local device defaults.
- `apps/agents/`: LangGraph agent helpers and LLM provider selection.

See `apps/agents/README.md` for agent setup and `test/agents/` for live-device scripts.

## Known Limitations

- No authentication is implemented. Use only on trusted networks.
- Some runtime configuration validation is duplicated between HTTP and MCP paths.
- The code builds against ArduinoJson v7 because `WiFiProvisioner` requires v7 APIs; some local MCP handlers still use compatibility APIs that emit deprecation warnings.
- `apps/tools/run_scrape.py` currently contains local, hardcoded device and OTLP endpoint defaults.
- The live-device tests require an ESP32 already flashed, connected, and reachable on the network.
