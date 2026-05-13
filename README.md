# ESP32 IoT Server Repository

This repository now uses `esp32-modular-iot-server/` as the single active project.

## Project Map

- `esp32-modular-iot-server/`: Active ESP32 firmware (PlatformIO, Arduino framework)
- `esp32-modular-iot-server/apps/`: Python tooling (metrics scraper) and MCP/LLM agents

## Notes

- Legacy root folders `src/` and `src_old/` have been removed.
- Firmware development, build, and testing should be run from `esp32-modular-iot-server/`.
