#pragma once
#include <windows.h>
#include <string>
#include "WebView2.h"

class BrowserWindow {
public:
  BrowserWindow();
  ~BrowserWindow();

  bool Initialize(HWND parent);

  void Navigate(const std::string& url);
  void Navigate(const std::wstring& url);
  void GoBack();
  void GoForward();
  void Reload();
  void Resize();
  void ExportCookies();
  void ImportCookies();
  void ToggleTheme();
  bool IsDark() const { return isDark_; }

  void SavePassword();
  void AutoFillPassword();
  void ViewPasswords();
  void ClearPasswords();
  void ToggleAdBlock();
  bool IsAdBlockEnabled() const { return adBlockEnabled_; }

  ICoreWebView2* WebView() const { return webview_; }

private:
  ICoreWebView2Environment* env_ = nullptr;
  ICoreWebView2Controller* controller_ = nullptr;
  ICoreWebView2* webview_ = nullptr;
  bool isDark_ = false;
  bool adBlockEnabled_ = false;

  friend struct EnvHandler;
  friend struct CtrlHandler;
  friend struct PmSaveHandler;
  friend struct PmAutofillHandler;
};
