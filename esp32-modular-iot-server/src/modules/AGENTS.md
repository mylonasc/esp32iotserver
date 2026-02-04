# Modules

Modules are the public face of each peripheral. They:

- own a device controller
- register routes (`/thing`, `/api/thing`)
- render `Home` and `Config` fragments
- parse config POSTs
- implement MCP tools

Required methods (IModule):

- `name()`
- `begin(AppContext&)`
- `loop(AppContext&)`
- `registerRoutes(AppContext&)`

Recommended additions:

- `renderHome(AppContext&, Print&)`
- `renderConfig(AppContext&, Print&)`
- `writeApiStatusObject(AppContext&, Print&)`
- `appendMcpTools(...)` + `handleMcpToolCall(...)`

When adding a module:

1) Add config fields in `Config.h` + load/save in `Config.cpp`.
2) Implement module routes and UI fragments.
3) Register the module in `src/main.cpp` via `modules.add(...)`.
