#pragma once
#include <Arduino.h>

inline String htmlHeader(const String& title) {
  String s;
  s += "<!doctype html><html><head>";
  s += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  s += "<title>" + title + "</title>";
  s += "<style>";
  s += "body{font-family:Arial;margin:0;background:#eee;}";
  s += ".c{max-width:860px;margin:20px auto;padding:16px;background:#fff;border-radius:10px;}";
  s += "input,select{width:100%;padding:10px;margin:6px 0;box-sizing:border-box;}";
  s += "button{padding:10px 14px;margin:6px 6px 6px 0;}";
  s += ".box{border:1px solid #ddd;border-radius:10px;padding:12px;margin:10px 0;background:#fafafa;}";
  s += "details.box{padding:0;}";
  s += "details.box>summary{padding:12px;font-weight:bold;cursor:pointer;}";
  s += "details.box[open]>summary{border-bottom:1px solid #ddd;}";
  s += "details.box>summary+*{padding:12px;}";
  s += "a{margin-right:12px;}";
  s += "</style></head><body><div class='c'>";
  s += "<div>";
  s += "<a href='/'>Home</a>";
  s += "<a href='/watering_pumps'>Pumps</a>";
  s += "<a href='/diag'>Diagnostics</a>";
  s += "<a href='/config'>Config</a>";
  s += "</div><hr>";
  return s;
}

inline String htmlFooter() {
  return "</div></body></html>";
}
