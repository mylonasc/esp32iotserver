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

void TemplateModule::renderHome(AppContext& ctx, Print& out) {
  (void)ctx;
  out.print(F("<div class='box'>"));
  out.print(F("<b>Template</b><br>"));
  out.print(F("<a href='/template'>Open</a>"));
  out.print(F("</div>"));
}

void TemplateModule::renderConfig(AppContext& ctx, Print& out) {
  (void)ctx;
  out.print(F("<details class='box'>"));
  out.print(F("<summary>Template</summary>"));
  out.print(F("<p>Add your config fields here.</p>"));
  out.print(F("</details>"));
}

void TemplateModule::handleConfigPost(AppContext& ctx) {
  (void)ctx;
  // Parse ctx.server args and update ctx.config.<yourConfig>
  // Then apply immediately (re-init device/controller)
}

void TemplateModule::writeApiStatusObject(AppContext& ctx, Print& out) {
  (void)ctx;
  out.print(F("{\"ui\":\"/template\",\"api\":\"/api/template\"}"));
}

void TemplateModule::writeModuleInfoObject(AppContext& ctx, Print& out) {
  (void)ctx;
  out.print(F("{\"name\":\"template\",\"ui\":\"/template\",\"api\":\"/api/template\"}"));
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
