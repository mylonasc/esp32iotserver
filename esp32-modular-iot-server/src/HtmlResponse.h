#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "Html.h"

class HtmlResponseOut {
public:
  explicit HtmlResponseOut(WebServer& server) : server_(server) {}

  void begin(const __FlashStringHelper* title) {
    server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_.sendHeader(F("Cache-Control"), F("no-cache"));
    server_.send(200, "text/html", "");
    server_.sendContent(htmlHeader(title));
  }

  void end() {
    server_.sendContent(htmlFooter());
    server_.sendContent("");
  }

  void p(const __FlashStringHelper* t) { server_.sendContent(t); }
  void p(const String& t) { server_.sendContent(t); }
  void p(const char* t) { server_.sendContent(t); }

  void pInt(int v) {
    char buf[16];
    itoa(v, buf, 10);
    server_.sendContent(buf);
  }

  void pFloat(float v, int decimals) {
    char buf[24];
    dtostrf(v, 0, decimals, buf);
    char* p = buf;
    while (*p == ' ') ++p;
    server_.sendContent(p);
  }

private:
  WebServer& server_;
};
