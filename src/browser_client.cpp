#include "browser_client.h"
#include "main.h"

#include <string>

void BrowserClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title) {
  CEF_REQUIRE_UI_THREAD();
  std::string titleStr = title.ToString() + " - MyBrowser";
  SetWindowTextA(gMainHwnd, titleStr.c_str());
}

void BrowserClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  browser_ = browser;
  browser_count_++;
}

bool BrowserClient::DoClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  return false; // let CEF handle the close
}

void BrowserClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  browser_ = nullptr;
  browser_count_--;
  if (browser_count_ == 0) {
    CefQuitMessageLoop();
  }
}

void BrowserClient::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                         bool isLoading,
                                         bool canGoBack,
                                         bool canGoForward) {
  CEF_REQUIRE_UI_THREAD();
  // Enable/disable toolbar buttons
  EnableWindow(GetDlgItem(gMainHwnd, kIdBackButton), canGoBack ? TRUE : FALSE);
  EnableWindow(GetDlgItem(gMainHwnd, kIdForwardButton), canGoForward ? TRUE : FALSE);
  EnableWindow(GetDlgItem(gMainHwnd, kIdReloadButton), TRUE);
}

void BrowserClient::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              int httpStatusCode) {
  CEF_REQUIRE_UI_THREAD();
  if (frame->IsMain()) {
    // Update the address bar
    std::string url = frame->GetURL().ToString();
    SetWindowTextA(GetDlgItem(gMainHwnd, kIdAddressBar), url.c_str());
  }
}
