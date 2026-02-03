#pragma once
#include "IModule.h"
#include "devices/PumpController.h"

class HtmlOut; // forward-declare; defined in PumpModule.cpp

class PumpModule : public IModule {
public:
  const char* name() const override { return "pumps"; }

  void begin(AppContext& ctx) override;
  void loop(AppContext& ctx) override;
  void registerRoutes(AppContext& ctx) override;

  // Updated IModule uses Print-based render hooks
  void renderHome(AppContext& ctx, Print& out) override;
  void renderConfig(AppContext& ctx, Print& out) override;
  void handleConfigPost(AppContext& ctx) override;

  void writeApiStatusObject(AppContext& ctx, Print& out) override;
  void writeModuleInfoObject(AppContext& ctx, Print& out) override;

private:
  struct PumpActionResult {
    bool hasMessage = false;
    bool isError = false;
    char message[160] = {0};
  };

  PumpController pumps_;
  AppContext* ctx_ = nullptr;

  void handlePumpsPage_(AppContext& ctx);
  void handlePumpsApi_(AppContext& ctx);

  // Clean page helpers
  PumpActionResult processPumpAction_(AppContext& ctx);
  void renderStatusBox_(HtmlOut& out);
  void renderControlForm_(HtmlOut& out, AppContext& ctx);
  void renderActionMessage_(HtmlOut& out, const PumpActionResult& r);
};
