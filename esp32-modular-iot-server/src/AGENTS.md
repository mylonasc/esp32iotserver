# ESP32 Modular Server Design

This codebase uses a device+module architecture:

- `devices/` hold low-level hardware controllers (pins, ADC, timing).
- `modules/` expose devices via HTTP/UI/MCP and persist config.
- `ModuleManager` aggregates module hooks for boot, loop, UI, and MCP.

Core flow:

1) `AppConfig` loads from `ConfigStore`.
2) Modules are added to `ModuleManager` and initialized via `beginAll`.
3) Web UI renders shared pages and lets modules contribute sections.
4) MCP `/mcp` exposes per-module tools and is delegated via `ModuleManager`.

Extension tips:

- Prefer `Print&` streaming for HTML/JSON to reduce heap fragmentation.
- Avoid blocking work in `loop()`; use `millis()` timers.
- Keep config keys stable; group by module prefix.
