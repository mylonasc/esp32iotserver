#pragma once
#include "IModule.h"
#include "devices/PumpController.h"

class PumpModule : public IModule {
public:
  const char* name() const override { return "pumps"; }

  void begin(AppContext& ctx) override;
  void loop(AppContext& ctx) override;
  void registerRoutes(AppContext& ctx) override;

  void renderHome(AppContext& ctx, String& html) override;
  void renderConfig(AppContext& ctx, String& html) override;
  void handleConfigPost(AppContext& ctx) override;

private:
  PumpController pumps_;

  void handlePumpsPage_(AppContext& ctx);
  void handlePumpsApi_(AppContext& ctx);
};
