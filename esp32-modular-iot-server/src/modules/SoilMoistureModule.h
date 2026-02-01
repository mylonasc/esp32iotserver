#pragma once
#include "IModule.h"
#include "devices/SoilMoistureController.h"

class SoilMoistureModule : public IModule {
public:
  const char* name() const override { return "soil"; }

  void begin(AppContext& ctx) override;
  void loop(AppContext& ctx) override;
  void registerRoutes(AppContext& ctx) override;

  void renderHome(AppContext& ctx, String& html) override;
  void renderConfig(AppContext& ctx, String& html) override;
  void handleConfigPost(AppContext& ctx) override;

  void appendApiStatusObject(AppContext& ctx, String& json) override;
  void appendModuleInfoObject(AppContext& ctx, String& json) override;

private:
  AppContext* ctx_ = nullptr;
  SoilMoistureController soil_;

  void handleSoilPage_(AppContext& ctx);
  void handleSoilApi_(AppContext& ctx);

  static void appendSensorRow_(String& html,
                              const SoilMoistureController::Reading& r,
                              uint32_t now);

  static void appendSensorJson_(String& json,
                              const SoilMoistureController::Reading& r,
                              uint32_t now);
};