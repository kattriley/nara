#pragma once
#include <windows.h>
#include "lang.h"

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
constexpr int kIdSpotifyButton    = 107;
constexpr int kIdDownloads        = 111;
constexpr int kIdSiteDark         = 112;
constexpr int kIdBack             = 113;
constexpr int kIdForward          = 114;
constexpr int kIdReload           = 115;
constexpr int kIdSearch           = 116;
constexpr int kIdAddrEdit         = 117;
constexpr int kIdGoAddr           = 118;
constexpr int kIdNewTab           = 119;
constexpr int kIdTabBase          = 500;
constexpr int kIdTabCloseBase     = 600;

constexpr int kIdAccelReload      = 1001;
constexpr UINT kMsgCreateTab     = WM_APP + 1;
constexpr int kIdToggleTheme      = 200;
constexpr int kIdLangEnglish      = 201;
constexpr int kIdLangDutch        = 202;
constexpr int kIdLangPolish       = 203;
constexpr int kIdCredits          = 204;

constexpr int kTabBarHeight = 28;
constexpr int kToolbarHeight = 30;
constexpr int kSiteRowHeight = 32;
constexpr int kBtnW = 72;
constexpr int kBtnH = 24;
constexpr int kBtnGap = 3;
constexpr int kBtnTop = 3;
constexpr int kNavBtnW = 26;

extern HWND gMainHwnd;
extern BrowserWindow* gBrowser;

HWND CreateMainWindow(HINSTANCE hInstance);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
