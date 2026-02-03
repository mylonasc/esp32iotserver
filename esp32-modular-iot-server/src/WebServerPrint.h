#pragma once
#include <Arduino.h>
#include <WebServer.h>

class WebServerPrint : public Print {
public:
  explicit WebServerPrint(WebServer& s) : s_(s) {}

  size_t write(uint8_t b) override {
    char c[2] = { static_cast<char>(b), 0 };
    s_.sendContent(c);
    return 1;
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    if (!buffer || !size) return 0;

    // WebServer::sendContent expects a C string; it also has overloads for String in some cores.
    // Safest: send as chunked pieces using a temporary stack buffer with NUL termination.
    // (Avoids heap, avoids assuming an overload exists.)
    constexpr size_t CHUNK = 128;
    char tmp[CHUNK + 1];

    size_t sent = 0;
    while (sent < size) {
      size_t n = size - sent;
      if (n > CHUNK) n = CHUNK;
      memcpy(tmp, buffer + sent, n);
      tmp[n] = 0;
      s_.sendContent(tmp);
      sent += n;
    }
    return size;
  }

private:
  WebServer& s_;
};