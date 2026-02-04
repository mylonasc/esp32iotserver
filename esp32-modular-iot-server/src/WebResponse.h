#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "WebServerPrint.h"
#include "Html.h"

class ChunkedResponse {
public:
  ChunkedResponse(WebServer& s, const char* contentType)
      : s_(s), out_(s) {
    s_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    s_.send(200, contentType, "");
  }

  ~ChunkedResponse() { end(); }

  WebServerPrint& out() { return out_; }

  void end() {
    if (ended_) return;
    s_.sendContent("");
    ended_ = true;
  }

private:
  WebServer& s_;
  WebServerPrint out_;
  bool ended_ = false;
};

inline ChunkedResponse beginChunkedHtml(WebServer& s, const __FlashStringHelper* title) {
  ChunkedResponse res(s, "text/html");
  res.out().print(htmlHeader(title));
  return res;
}

inline ChunkedResponse beginChunkedJson(WebServer& s) {
  return ChunkedResponse(s, "application/json");
}
