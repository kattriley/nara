#pragma once
#include <windows.h>

constexpr int kWindowWidth  = 800;
constexpr int kWindowHeight = 600;

constexpr int kIdGoogleButton     = 101;
constexpr int kIdPinterestButton  = 102;
constexpr int kIdRobloxButton     = 103;
constexpr int kIdDiscordButton    = 109;

constexpr int kIdSettingsButton   = 106;

constexpr int kIdAccelReload      = 1001;
constexpr int kIdToggleTheme      = 200;

constexpr int kToolbarHeight = 38;
constexpr int kBtnW = 72;
constexpr int kBtnH = 26;
constexpr int kBtnGap = 4;
constexpr int kBtnTop = 6;

extern HWND gMainHwnd;
extern bool gIsDark;

void NavigateFirefox(const wchar_t* url);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
