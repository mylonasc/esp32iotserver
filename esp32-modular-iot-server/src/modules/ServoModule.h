#pragma once
#include "IModule.h"
#include "devices/ServoController.h"

class ServoModule : public IModule {
public:
  const char* name() const override { return "servo"; }

  void begin(AppContext& ctx) override;
  void loop(AppContext& ctx) override;
  void registerRoutes(AppContext& ctx) override;

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
  AppContext* ctx_ = nullptr;
  ServoController servo_;

  void handleServoPage_(AppContext& ctx);
  void handleServoApi_(AppContext& ctx);
  void handleServoStart_(AppContext& ctx);
  void handleServoStop_(AppContext& ctx);
  void printConfigObject_(const ServoConfig& c, Print& out) const;
};
