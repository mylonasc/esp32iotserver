# README — Modular ESP32 Web Server (PlatformIO)

This project is an ESP32 Arduino + PlatformIO skeleton designed around a **module/plugin architecture**. You can add new “peripherals” (pumps, sensors, relays, lights, etc.) as **modules** that contribute:

* background logic (`loop()`)
* HTTP routes (`/something`, `/api/something`)
* UI sections on shared pages (`/` Home, `/config` Config)
* config parsing + persistence (via `AppConfig` + `ConfigStore`)

The goal is to **avoid signature explosion** (no more `WebUi.begin(server, cfg, pumps, servo, soil, ...)`) and keep everything extensible.

---

## Project structure

```
src/
  main.cpp                 # app entry point: wires wifi + web + modules
  AppContext.h             # shared “context” passed to all modules
  IModule.h                # module interface (hooks)
  ModuleManager.h          # holds and runs the list of installed modules
  Html.h                   # simple HTML helpers used by WebUi and modules

  Config.h / Config.cpp    # persistent configuration (Preferences)
  WifiManager.h / .cpp     # WiFi connect + provisioning fallback
  WebUi.h / WebUi.cpp      # shared pages (/ and /config), aggregates module UI

  devices/
    ...                    # reusable hardware controllers (no web/UI)
  modules/
    ...                    # modules that combine device logic + UI + routes
```

### Key idea

* **Devices** (`src/devices/`) are low-level controllers (pins, ADC reads, etc.).
* **Modules** (`src/modules/`) are the “plugins” that:

  * own device objects
  * register routes
  * render HTML fragments
  * parse config inputs

---

## How the app boots

1. `main.cpp` loads `AppConfig` from `ConfigStore` (Preferences).
2. `main.cpp` constructs an `AppContext` (server + config + wifi + store).
3. `main.cpp` installs modules using:

```cpp
modules.add(pumpModule);
modules.add(soilModule);
// etc.
```

4. Modules initialize early (`modules.beginAll(ctx)`) so pins start in safe states.
5. WiFi connects or falls back to provisioning portal.
6. Once connected, the web server starts:

   * WebUi registers core routes (`/`, `/config`, `/reset_wifi`)
   * ModuleManager registers module routes
7. `loop()` ticks:

   * wifi state machine
   * each module’s `loop()` (non-blocking)
   * `server.handleClient()` when connected

---

## Module creation: recommended pattern

A module should have **two files**:

* `src/modules/MyThingModule.h`
* `src/modules/MyThingModule.cpp`

It should implement the `IModule` interface:

```cpp
class IModule {
public:
  virtual const char* name() const = 0;
  virtual void begin(AppContext& ctx) = 0;
  virtual void loop(AppContext& ctx) = 0;
  virtual void registerRoutes(AppContext& ctx) = 0;

  // optional
  virtual void renderHome(AppContext& ctx, String& html);
  virtual void renderConfig(AppContext& ctx, String& html);
  virtual void handleConfigPost(AppContext& ctx);
};
```

### What goes where

**Device logic**

* Put reusable device logic in `src/devices/` (e.g., `RelayController`, `DhtController`, `SoilMoistureController`)
* That code should *not* know about `WebServer`, HTML, URLs, etc.
* It should be “pure control + readings” with `begin()` + `loop()`

**Module logic**

* Own the device controller as a member
* Read config from `ctx.config`
* Contribute UI and routes

---

## Step-by-step: create a new module

### Step 1 — Create a device controller (optional but recommended)

Example: `src/devices/RelayController.h/.cpp`

* Expose a small API: `begin(cfg)`, `setOn/off`, `toggle`, `getState`
* Keep it non-blocking; use `millis()` if you need timing

> If your module is tiny, you can embed everything in the module, but separating device code is better long-term.

---

### Step 2 — Add config fields

If your module needs persistent settings, add config fields to `AppConfig` in `src/Config.h`.

Example:

```cpp
struct RelayConfig {
  bool enabled = true;
  int pin = 23;
  String id = "light_1";
};

struct AppConfig {
  ...
  RelayConfig relay;
};
```

Then update `ConfigStore::load()` and `ConfigStore::save()` in `src/Config.cpp` using Preferences keys.

**Guidelines**

* Keep keys short and consistent: `relay_en`, `relay_pin`, `relay_id`
* Group keys by module name prefix to avoid collisions
* Save/load in the `NS_CONFIG` namespace (not WiFi settings)

---

### Step 3 — Create module files

`src/modules/RelayModule.h`:

```cpp
#pragma once
#include "IModule.h"
#include "devices/RelayController.h"

class RelayModule : public IModule {
public:
  const char* name() const override { return "relay"; }

  void begin(AppContext& ctx) override;
  void loop(AppContext& ctx) override;
  void registerRoutes(AppContext& ctx) override;

  void renderHome(AppContext& ctx, String& html) override;
  void renderConfig(AppContext& ctx, String& html) override;
  void handleConfigPost(AppContext& ctx) override;

private:
  RelayController relay_;

  void handleRelayPage_(AppContext& ctx);
  void handleRelayApi_(AppContext& ctx);
};
```

`src/modules/RelayModule.cpp` should:

* initialize device from config in `begin()`
* tick the device in `loop()`
* register its routes in `registerRoutes()`
* add a Home status box in `renderHome()`
* add config fields in `renderConfig()`
* parse POSTed config in `handleConfigPost()`

---

### Step 4 — Register module routes

Inside `registerRoutes()`:

```cpp
void RelayModule::registerRoutes(AppContext& ctx) {
  ctx.server.on("/relay", HTTP_GET, [&ctx, this]() { handleRelayPage_(ctx); });
  ctx.server.on("/api/relay", HTTP_GET, [&ctx, this]() { handleRelayApi_(ctx); });
}
```

**Rules of thumb**

* UI page: `/relay`, `/lights`, `/dht`, `/soil`, ...
* JSON endpoint: `/api/relay`, `/api/soil`, ...
* Keep route names stable. Avoid changing them often.

---

### Step 5 — Contribute UI to Home and Config

#### Home (`/`)

Add a compact status box:

```cpp
void RelayModule::renderHome(AppContext& ctx, String& html) {
  html += "<div class='box'>";
  html += "<b>Relay</b><br>";
  html += "State: " + String(relay_.isOn() ? "ON" : "OFF");
  html += "<br><a href='/relay'>Open</a>";
  html += "</div>";
}
```

#### Config (`/config`)

Add module-specific input fields:

```cpp
void RelayModule::renderConfig(AppContext& ctx, String& html) {
  auto& c = ctx.config.relay;
  html += "<div class='box'><h3>Relay</h3>";
  html += "<label><input type='checkbox' name='relay_en' ";
  html += (c.enabled ? "checked" : "");
  html += "> Enabled</label>";
  html += "<label>Pin</label>";
  html += "<input type='number' name='relay_pin' value='" + String(c.pin) + "'>";
  html += "<label>ID</label>";
  html += "<input name='relay_id' value='" + c.id + "'>";
  html += "</div>";
}
```

#### Config POST handling

Parse submitted fields:

```cpp
void RelayModule::handleConfigPost(AppContext& ctx) {
  auto& s = ctx.server;
  auto& c = ctx.config.relay;

  c.enabled = s.hasArg("relay_en");
  if (s.hasArg("relay_pin")) c.pin = s.arg("relay_pin").toInt();
  if (s.hasArg("relay_id"))  c.id = s.arg("relay_id");

  relay_.begin(c); // apply immediately
}
```

**Persistence**

* WebUi saves the full config once after calling all modules’ `handleConfigPost()`:

  * `ctx.configStore.save(ctx.config);`
* WebUi typically reboots after saving for clean re-init. (You can later optimize to avoid reboots.)

---

### Step 6 — Install your module in `main.cpp`

Include + instantiate + add:

```cpp
#include "modules/RelayModule.h"

RelayModule relayModule;

void setup() {
  ...
  modules.add(pumpModule);
  modules.add(soilModule);
  modules.add(relayModule);
  ...
}
```

That’s it. No other signatures change.

---

## Design conventions

### Prefer “device + module”

* **Controller**: hardware behavior (pin setup, readings, timing)
* **Module**: web routes + UI + config + ownership of controller

### Keep `loop()` non-blocking

Use `millis()` timers, not `delay()` (except tiny delays like 2ms in the main loop).

### Avoid dynamic memory in hot paths

* Avoid `new`/`delete` in loops
* Try not to build enormous `String`s repeatedly at high frequency
* For JSON, keep it small or serve only on request (which you already do)

### Route naming

* `/thing` for HTML page
* `/api/thing` for JSON
* Keep module names unique

### Config key naming

* Prefix keys with module name:

  * `soil0_pin`, `soil_int`
  * `relay_pin`, `relay_en`
* Keep keys stable so old configs continue to work after firmware updates

---

## Typical module capabilities

### Monitoring / periodic sampling

In the device controller:

* store `lastReadMs`
* if `now - lastReadMs >= interval`, read again

### API output

Return a JSON object containing:

* module status
* readings
* sensor metadata (id/pin/enabled)
* timestamps/age for readings

### UI rendering

* Home should be “summary”
* A dedicated page should show “details”

---

## Common pitfalls

### “request handler not found”

Browsers hit `/favicon.ico` and other paths. Add `server.onNotFound(...)` once in `startServices()`.

### ESP32 ADC quirks

Soil sensors use ADC. Readings vary by board/pin/attenuation. Add calibration fields or attenuation settings if needed.

### Pin safety

Initialize pins in `begin()` with safe defaults:

* outputs to LOW
* inputs to INPUT

---

