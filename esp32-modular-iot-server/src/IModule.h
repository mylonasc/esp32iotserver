#pragma once

#include <Arduino.h>
#include "AppContext.h"

class IModule {
public:
  virtual ~IModule() = default;

  // stable module identifier (used in /api aggregation)
  virtual const char* name() const = 0;

  virtual void begin(AppContext& ctx) = 0;
  virtual void loop(AppContext& ctx) = 0;
  virtual void registerRoutes(AppContext& ctx) = 0;

  // --------------------------------------------------------------------------
  // New preferred API: stream to Print sinks (low fragmentation).
  //
  // Implement these in new/updated modules.
  // The caller (aggregator) decides whether Print is a network client,
  // a response stream, or even a StringPrint wrapper.
  // --------------------------------------------------------------------------

  // Optional: contribute fragments to shared pages
  virtual void renderHome(AppContext& ctx, Print& out)    { (void)ctx; (void)out; }
  virtual void renderConfig(AppContext& ctx, Print& out)  { (void)ctx; (void)out; }
  virtual void handleConfigPost(AppContext& ctx)          { (void)ctx; }

  // Contribute a JSON object for /api under key = module name().
  // Implementations should print a JSON object only: {"running":true,...}
  virtual void writeApiStatusObject(AppContext& ctx, Print& out) {
    (void)ctx;
    out.print(F("{}"));
  }

  // Contribute one entry object to /api/modules array.
  // Default provides {"name":"..."}; modules can add ui/api paths, etc.
  virtual void writeModuleInfoObject(AppContext& ctx, Print& out) {
    (void)ctx;
    out.print(F("{\"name\":\""));
    out.print(name());
    out.print(F("\"}"));
  }

  // --------------------------------------------------------------------------
  // Back-compat layer: old String-based hooks.
  //
  // Keep these while migrating modules. New code should call the Print-based
  // versions; old modules can still override these until updated.
  // --------------------------------------------------------------------------
#if !defined(IMODULE_DISABLE_STRING_API)
  virtual void renderHome(AppContext& ctx, String& html) {
    // Default: delegate to Print-based version via String append
    class StringPrint : public Print {
    public:
      explicit StringPrint(String& s) : s_(s) {}
      size_t write(uint8_t b) override { s_ += (char)b; return 1; }
      size_t write(const uint8_t* buf, size_t size) override {
        if (!buf || !size) return 0;
        // Avoid per-byte churn by appending in a single step
        // (String::concat copies; still allocates, but less chatty)
        s_.concat((const char*)buf, size);
        return size;
      }
    private:
      String& s_;
    };

    StringPrint sp(html);
    renderHome(ctx, sp);
  }

  virtual void renderConfig(AppContext& ctx, String& html) {
    class StringPrint : public Print {
    public:
      explicit StringPrint(String& s) : s_(s) {}
      size_t write(uint8_t b) override { s_ += (char)b; return 1; }
      size_t write(const uint8_t* buf, size_t size) override {
        if (!buf || !size) return 0;
        s_.concat((const char*)buf, size);
        return size;
      }
    private:
      String& s_;
    };

    StringPrint sp(html);
    renderConfig(ctx, sp);
  }

  virtual void appendApiStatusObject(AppContext& ctx, String& json) {
    class StringPrint : public Print {
    public:
      explicit StringPrint(String& s) : s_(s) {}
      size_t write(uint8_t b) override { s_ += (char)b; return 1; }
      size_t write(const uint8_t* buf, size_t size) override {
        if (!buf || !size) return 0;
        s_.concat((const char*)buf, size);
        return size;
      }
    private:
      String& s_;
    };

    StringPrint sp(json);
    writeApiStatusObject(ctx, sp);
  }

  virtual void appendModuleInfoObject(AppContext& ctx, String& json) {
    class StringPrint : public Print {
    public:
      explicit StringPrint(String& s) : s_(s) {}
      size_t write(uint8_t b) override { s_ += (char)b; return 1; }
      size_t write(const uint8_t* buf, size_t size) override {
        if (!buf || !size) return 0;
        s_.concat((const char*)buf, size);
        return size;
      }
    private:
      String& s_;
    };

    StringPrint sp(json);
    writeModuleInfoObject(ctx, sp);
  }
#endif
};
