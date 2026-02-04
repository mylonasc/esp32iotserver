#pragma once
#include "IModule.h"
#include "devices/SoilMoistureController.h"

class SoilMoistureModule : public IModule {
public:
  const char* name() const override { return "soil"; }

  void begin(AppContext& ctx) override;
  void loop(AppContext& ctx) override;
  void registerRoutes(AppContext& ctx) override;

  void renderHome(AppContext& ctx, Print& out) override;
  void renderConfig(AppContext& ctx, Print& out) override;
  void writeApiStatusObject(AppContext& ctx, Print& out) override;
  void writeModuleInfoObject(AppContext& ctx, Print& out) override;

  void renderHome(AppContext& ctx, String& html) override;
  void renderConfig(AppContext& ctx, String& html) override;
  void handleConfigPost(AppContext& ctx) override;

  void appendApiStatusObject(AppContext& ctx, String& json) override;
  void appendModuleInfoObject(AppContext& ctx, String& json) override;

  const SoilMoistureController& controller() const { return soil_; }

  void appendMcpTools(Print& out, bool& first) override;
  bool supportsMcpTool(const char* toolName) const override;
  bool handleMcpToolCall(AppContext& ctx,
                         const char* toolName,
                         JsonObject args,
                         Print& out,
                         bool& rebootRequested) override;

private:
  AppContext* ctx_ = nullptr;
  SoilMoistureController soil_;

  void handleSoilPage_(AppContext& ctx);
  void handleSoilApi_(AppContext& ctx);

  bool hasEnabledSensors_() const;

  static void appendSensorRow_(Print& out,
                              const SoilMoistureController::Reading& r,
                              uint32_t now);

  static void appendSensorJson_(Print& out,
                              const SoilMoistureController::Reading& r,
                              uint32_t now);
};
