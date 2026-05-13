#pragma once
#include "IModule.h"

// Rename TemplateModule -> YourModuleName
class TemplateModule : public IModule {
public:
  const char* name() const override { return "template"; }

  void begin(AppContext& ctx) override;
  void loop(AppContext& ctx) override;
  void registerRoutes(AppContext& ctx) override;

  void renderHome(AppContext& ctx, Print& out) override;
  void renderConfig(AppContext& ctx, Print& out) override;
  void handleConfigPost(AppContext& ctx) override;

  void writeApiStatusObject(AppContext& ctx, Print& out) override;
  void writeModuleInfoObject(AppContext& ctx, Print& out) override;

private:
  AppContext* ctx_ = nullptr;

  // Optional: your internal state / device controller objects go here

  void handleUiPage_(AppContext& ctx);
  void handleApi_(AppContext& ctx);
};
