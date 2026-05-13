# ESP32 Modular Refactor Runbook

This runbook executes the agreed cleanup and simplification plan in small, reviewable phases.

## Phase 0 - Safety and Baseline

1. Check repo status and current branch.
2. Confirm active project path is `esp32-modular-iot-server/`.
3. Capture a baseline build result from `esp32-modular-iot-server/`.

Acceptance:
- Repo status is understood before changes.
- Active firmware path is explicit.

## Phase 1 - Repository Cleanup (Current)

Scope:
- Remove root legacy directories `src/` and `src_old/`.
- Add root documentation that points contributors to the active project.

Steps:
1. Delete `src/` and `src_old/`.
2. Add/update root `README.md` with project map and legacy removal note.
3. Add this runbook (`RUNBOOK.md`).
4. Verify no expected root legacy folders remain.

Acceptance:
- Root contains no `src/` or `src_old/`.
- `README.md` points to `esp32-modular-iot-server/` as source of truth.

## Phase 2 - API Surface Simplification

Scope:
- Remove String-based compatibility layer from modular interfaces.

Targets:
- `esp32-modular-iot-server/src/IModule.h`
- `esp32-modular-iot-server/src/ModuleManager.h`
- `esp32-modular-iot-server/src/modules/SoilMoistureModule.*`
- `esp32-modular-iot-server/src/modules/TemplateModule.*`

Steps:
1. Remove old `String&` render/status hooks.
2. Keep only `Print&` streaming hooks.
3. Compile and fix any module signature mismatches.

Acceptance:
- No legacy `String` compatibility hooks remain in active module API.
- Firmware compiles.

## Phase 3 - Response and Route Unification

Scope:
- Consolidate duplicate response helper patterns.
- Ensure all UI-linked routes are registered.

Targets:
- `esp32-modular-iot-server/src/WebUi.*`
- `esp32-modular-iot-server/src/modules/PumpModule.cpp`
- `esp32-modular-iot-server/src/WebResponse.h`

Steps:
1. Standardize chunked/HTML response lifecycle.
2. Remove duplicate helper classes where practical.
3. Ensure `/reset_wifi` is either registered or no longer exposed.

Acceptance:
- Response finalization behavior is consistent.
- No dead UI links to missing routes.

## Phase 4 - Config and Validation Hardening

Scope:
- Refactor load/save organization and add input validation.

Targets:
- `esp32-modular-iot-server/src/Config.cpp`
- Module config POST handlers and MCP config set handlers.

Steps:
1. Split config load/save into helper blocks by module domain.
2. Add common range/pin validation utilities.
3. Apply the same validation from HTTP and MCP paths.

Acceptance:
- Config persistence code is easier to follow.
- Invalid values are clamped/rejected consistently.

## Phase 5 - Tooling and Ops Cleanup

Scope:
- Improve ignores and externalize scraper runtime config.

Targets:
- `esp32-modular-iot-server/.gitignore`
- `esp32-modular-iot-server/apps/tools/run_scrape.py`

Steps:
1. Expand ignore patterns for Python/build/editor artifacts.
2. Replace hardcoded devices/OTEL URL with env-driven or file-driven config.

Acceptance:
- Fewer accidental artifacts in git status.
- Scraper configuration is portable across environments.

## Verification Checklist (Each Phase)

1. `git status --short`
2. Firmware build from `esp32-modular-iot-server/`.
3. Basic endpoint smoke checks after firmware flashes and connects:
   - `/`
   - `/config`
   - `/api`
   - `/api/modules`
   - `/metrics`
   - `/mcp`
