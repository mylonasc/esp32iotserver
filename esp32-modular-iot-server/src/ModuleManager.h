#pragma once
#include "IModule.h"

// Fixed-size list: avoids heap allocations and surprises
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

private:
  static constexpr int MAX_ = 12;
  IModule* modules_[MAX_] = {nullptr};
  int count_ = 0;
};
