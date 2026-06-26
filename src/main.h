#pragma once
#include <windows.h>

class BrowserWindow;

constexpr int kWindowWidth  = 1280;
constexpr int kWindowHeight = 800;

constexpr int kIdGoogleButton     = 101;
constexpr int kIdPinterestButton  = 102;
constexpr int kIdRobloxButton     = 103;
constexpr int kIdDiscordButton    = 109;
constexpr int kIdImportCookies    = 104;
constexpr int kIdExportCookies    = 105;
constexpr int kIdSettingsButton   = 106;
constexpr int kIdPasswordsButton  = 107;
constexpr int kIdImportChrome     = 108;

constexpr int kIdAccelReload      = 1001;

constexpr int kIdToggleTheme      = 200;
constexpr int kIdPmSave           = 201;
constexpr int kIdPmAutofill       = 202;
constexpr int kIdPmView           = 203;
constexpr int kIdPmClear          = 204;

constexpr int kToolbarHeight = 38;
constexpr int kBtnW = 72;
constexpr int kBtnH = 26;
constexpr int kBtnGap = 4;
constexpr int kBtnTop = 6;

extern HWND gMainHwnd;
extern BrowserWindow* gBrowser;

HWND CreateMainWindow(HINSTANCE hInstance);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
