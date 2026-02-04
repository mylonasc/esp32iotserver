#pragma once
#include "IModule.h"
#include "devices/DhtController.h"

class DhtModule : public IModule {
public:
  const char* name() const override { return "dht"; }

  void begin(AppContext& ctx) override;
  void loop(AppContext& ctx) override;
  void registerRoutes(AppContext& ctx) override;

  void renderHome(AppContext& ctx, Print& out) override;
  void renderConfig(AppContext& ctx, Print& out) override;
  void handleConfigPost(AppContext& ctx) override;

  void writeApiStatusObject(AppContext& ctx, Print& out) override;
  void writeModuleInfoObject(AppContext& ctx, Print& out) override;

  void appendMcpTools(Print& out, bool& first) override;
  bool supportsMcpTool(const char* toolName) const override;
  bool handleMcpToolCall(AppContext& ctx,
                         const char* toolName,
                         JsonObject args,
                         Print& out,
                         bool& rebootRequested) override;

private:
  AppContext* ctx_ = nullptr;
  DhtController dht_;

  void handleDhtPage_(AppContext& ctx);
  void handleDhtApi_(AppContext& ctx);
};
