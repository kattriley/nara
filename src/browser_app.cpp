#include "browser_app.h"
#include "browser_client.h"
#include "main.h"

#include "include/cef_browser.h"
#include "include/cef_command_line.h"

void BrowserApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  // Parse command-line to get initial URL (default: google)
  CefRefPtr<CefCommandLine> cmd = CefCommandLine::GetGlobalCommandLine();
  std::string url = cmd->HasSwitch("url")
                        ? cmd->GetSwitchValue("url")
                        : "https://www.google.com";

  // Window info
  CefWindowInfo window_info;
  RECT rect;
  GetClientRect(gMainHwnd, &rect);

  // The browser will be a child of the client area below the toolbar
  constexpr int kToolbarHeight = 36;
  rect.top += kToolbarHeight;

  window_info.SetAsChild(gMainHwnd, rect);

  // Browser settings
  CefBrowserSettings settings;

  // Create the client handler
  CefRefPtr<BrowserClient> client(new BrowserClient());

  // Create the browser
  CefBrowserHost::CreateBrowser(window_info, client, url, settings, nullptr, nullptr);
}
