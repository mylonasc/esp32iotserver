# Devices

Devices are low-level controllers with no web knowledge.

Guidelines:

- Keep APIs small (`begin(cfg)`, `loop()`, getters).
- Use `millis()` instead of `delay()` for timing.
- Avoid dynamic allocation in fast paths.
- Expose structured readings for modules to render.

Devices should be reusable across modules.
