#include "TemplateModule.h"
#include "Html.h"
#include "WebResponse.h"

void TemplateModule::begin(AppContext& ctx) {
  ctx_ = &ctx;
  // Initialize your devices from ctx.config.<yourConfig>
}

void TemplateModule::loop(AppContext& ctx) {
  (void)ctx;
  // Non-blocking periodic work using millis()
}

void TemplateModule::registerRoutes(AppContext& ctx) {
  ctx_ = &ctx;

  // UI page
  ctx.server.on("/template", HTTP_GET, [this]() { handleUiPage_(*ctx_); });

  // API endpoint
  ctx.server.on("/api/template", HTTP_GET, [this]() { handleApi_(*ctx_); });
}

void TemplateModule::renderHome(AppContext& ctx, String& html) {
  (void)ctx;
  html += "<div class='box'>";
  html += "<b>Template</b><br>";
  html += "<a href='/template'>Open</a>";
  html += "</div>";
}

void TemplateModule::renderConfig(AppContext& ctx, String& html) {
  (void)ctx;
  html += "<details class='box'>";
  html += "<summary>Template</summary>";
  html += "<p>Add your config fields here.</p>";
  html += "</details>";
}

void TemplateModule::handleConfigPost(AppContext& ctx) {
  (void)ctx;
  // Parse ctx.server args and update ctx.config.<yourConfig>
  // Then apply immediately (re-init device/controller)
}

void TemplateModule::appendApiStatusObject(AppContext& ctx, String& json) {
  (void)ctx;
  json += "{";
  json += "\"ui\":\"/template\",";
  json += "\"api\":\"/api/template\"";
  json += "}";
}

void TemplateModule::appendModuleInfoObject(AppContext& ctx, String& json) {
  (void)ctx;
  json += "{";
  json += "\"name\":\"template\",";
  json += "\"ui\":\"/template\",";
  json += "\"api\":\"/api/template\"";
  json += "}";
}

void TemplateModule::handleUiPage_(AppContext& ctx) {
  auto res = beginChunkedHtml(ctx.server, F("Template"));
  auto& out = res.out();
  out.print(F("<h2>Template Module</h2>"));
  out.print(F("<p>This is a placeholder UI page.</p>"));
  out.print(F("<p><a href='/api/template'>JSON</a></p>"));
  out.print(htmlFooter());
}

void TemplateModule::handleApi_(AppContext& ctx) {
  auto res = beginChunkedJson(ctx.server);
  auto& out = res.out();
  out.print(F("{"));
  out.print(F("\"name\":\"template\","));
  out.print(F("\"ok\":true"));
  out.print(F("}"));
}
