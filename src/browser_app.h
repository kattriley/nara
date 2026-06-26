#pragma once

#include "include/cef_app.h"

// CefApp subclass – handles process-level callbacks.
// Required by CEF for all processes (browser, renderer, etc.).
class BrowserApp : public CefApp,
                   public CefBrowserProcessHandler {
public:
  BrowserApp() = default;

  // CefApp overrides
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  // CefBrowserProcessHandler override
  void OnContextInitialized() override;

private:
  IMPLEMENT_REFCOUNTING(BrowserApp);
  DISALLOW_COPY_AND_ASSIGN(BrowserApp);
};
