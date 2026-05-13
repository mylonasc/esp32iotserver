#pragma once
#include "IModule.h"
#include "devices/PumpController.h"

class HtmlResponseOut;

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
  void writeMetrics(AppContext& ctx, Print& out) override;

  void appendMcpTools(Print& out, bool& first) override;
  bool supportsMcpTool(const char* toolName) const override;
  bool handleMcpToolCall(AppContext& ctx,
                         const char* toolName,
                         JsonObject args,
                         Print& out,
                         bool& rebootRequested) override;

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
  void renderStatusBox_(HtmlResponseOut& out);
  void renderControlForm_(HtmlResponseOut& out, AppContext& ctx);
  void renderActionMessage_(HtmlResponseOut& out, const PumpActionResult& r);
};
