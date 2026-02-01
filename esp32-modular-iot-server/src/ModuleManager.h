#pragma once
#include "IModule.h"

class ModuleManager {
public:
  void add(IModule& m) {
    if (count_ < MAX_) modules_[count_++] = &m;
  }

  void beginAll(AppContext& ctx) {
    for (int i = 0; i < count_; ++i) modules_[i]->begin(ctx);
  }

  void loopAll(AppContext& ctx) {
    for (int i = 0; i < count_; ++i) modules_[i]->loop(ctx);
  }

  void registerAllRoutes(AppContext& ctx) {
    for (int i = 0; i < count_; ++i) modules_[i]->registerRoutes(ctx);
  }

  void renderHome(AppContext& ctx, String& html) {
    for (int i = 0; i < count_; ++i) modules_[i]->renderHome(ctx, html);
  }

  void renderConfig(AppContext& ctx, String& html) {
    for (int i = 0; i < count_; ++i) modules_[i]->renderConfig(ctx, html);
  }

  void handleConfigPost(AppContext& ctx) {
    for (int i = 0; i < count_; ++i) modules_[i]->handleConfigPost(ctx);
  }

  void appendAllApiStatus(AppContext& ctx, String& json) {
    bool first = true;
    for (int i = 0; i < count_; ++i) {
      if (!first) json += ",";
      first = false;

      json += "\"";
      json += modules_[i]->name();
      json += "\":";
      modules_[i]->appendApiStatusObject(ctx, json);
    }
  }

  void appendAllModuleInfo(AppContext& ctx, String& json) {
    bool first = true;
    for (int i = 0; i < count_; ++i) {
      if (!first) json += ",";
      first = false;
      modules_[i]->appendModuleInfoObject(ctx, json);
    }
  }

private:
  static constexpr int MAX_ = 12;
  IModule* modules_[MAX_] = {nullptr};
  int count_ = 0;
};
