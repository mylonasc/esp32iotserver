#pragma once
#include <Arduino.h>
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

  // --------------------------------------------------------------------------
  // Preferred: streaming HTML fragments (low fragmentation)
  // --------------------------------------------------------------------------
  void renderHome(AppContext& ctx, Print& out) {
    for (int i = 0; i < count_; ++i) modules_[i]->renderHome(ctx, out);
  }

  void renderConfig(AppContext& ctx, Print& out) {
    for (int i = 0; i < count_; ++i) modules_[i]->renderConfig(ctx, out);
  }

  void writeAllApiStatus(AppContext& ctx, Print& out) {
    bool first = true;
    for (int i = 0; i < count_; ++i) {
      if (!first) out.print(',');
      first = false;

      out.print('\"');
      out.print(modules_[i]->name()); // module names should be safe identifiers
      out.print(F("\":"));
      modules_[i]->writeApiStatusObject(ctx, out);
    }
  }

  // Produces array body:  {...},{...}
  // Caller is responsible for surrounding [ ].
  void writeAllModuleInfo(AppContext& ctx, Print& out) {
    bool first = true;
    for (int i = 0; i < count_; ++i) {
      if (!first) out.print(',');
      first = false;

      modules_[i]->writeModuleInfoObject(ctx, out);
    }
  }

  // --------------------------------------------------------------------------
  // MCP helpers
  // --------------------------------------------------------------------------
  void appendMcpTools(Print& out, bool& first) {
    for (int i = 0; i < count_; ++i) {
      modules_[i]->appendMcpTools(out, first);
    }
  }

  bool supportsMcpTool(const char* toolName) const {
    for (int i = 0; i < count_; ++i) {
      if (modules_[i]->supportsMcpTool(toolName)) return true;
    }
    return false;
  }

  bool handleMcpToolCall(AppContext& ctx,
                         const char* toolName,
                         JsonObject args,
                         Print& out,
                         bool& rebootRequested) {
    for (int i = 0; i < count_; ++i) {
      if (modules_[i]->handleMcpToolCall(ctx, toolName, args, out, rebootRequested)) {
        return true;
      }
    }
    return false;
  }

  // --------------------------------------------------------------------------
  // Config POST handlers stay as-is (not a fragmentation hotspot)
  // --------------------------------------------------------------------------
  void handleConfigPost(AppContext& ctx) {
    for (int i = 0; i < count_; ++i) modules_[i]->handleConfigPost(ctx);
  }

#if !defined(MODULEMANAGER_DISABLE_STRING_API)
  // --------------------------------------------------------------------------
  // Back-compat: String-based callers can still use these.
  // These are thin adapters that go through Print-based path.
  // Note: still can allocate due to String growth, but avoids duplicate logic.
  // --------------------------------------------------------------------------

  void renderHome(AppContext& ctx, String& html) {
    StringPrint sp(html);
    renderHome(ctx, sp);
  }

  void renderConfig(AppContext& ctx, String& html) {
    StringPrint sp(html);
    renderConfig(ctx, sp);
  }

  void appendAllApiStatus(AppContext& ctx, String& json) {
    StringPrint sp(json);
    writeAllApiStatus(ctx, sp);
  }

  void appendAllModuleInfo(AppContext& ctx, String& json) {
    StringPrint sp(json);
    writeAllModuleInfo(ctx, sp);
  }
#endif

private:
  static constexpr int MAX_ = 12;
  IModule* modules_[MAX_] = {nullptr};
  int count_ = 0;

#if !defined(MODULEMANAGER_DISABLE_STRING_API)
  // Local helper for adapting Print output into a String efficiently.
  class StringPrint : public Print {
  public:
    explicit StringPrint(String& s) : s_(s) {}

    size_t write(uint8_t b) override {
      s_ += static_cast<char>(b);
      return 1;
    }

    size_t write(const uint8_t* buf, size_t size) override {
      if (!buf || !size) return 0;
      // Append in one call to avoid per-byte overhead
      s_.concat(reinterpret_cast<const char*>(buf), size);
      return size;
    }

  private:
    String& s_;
  };
#endif
};
