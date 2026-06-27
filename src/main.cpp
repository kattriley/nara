#include "main.h"
#include "browser_window.h"
#include <urlmon.h>
#include <commctrl.h>
#include <windowsx.h>

#pragma comment(lib, "comctl32.lib")

HWND gMainHwnd = nullptr;
BrowserWindow* gBrowser = nullptr;

static HBRUSH gToolbarBrush = nullptr;
static HBRUSH gSiteRowBrush = nullptr;
static HFONT gToolbarFont = nullptr;
static HFONT gAddrFont = nullptr;
static HFONT gTabFont = nullptr;

static void LoadToolbarFont() {
  const wchar_t* fontName = L"Segoe UI Variable";
  const wchar_t* fallback = L"Segoe UI";

  HKEY hKey;
  wchar_t fontPath[MAX_PATH];
  bool found = false;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    wchar_t valueName[256];
    DWORD valNameSz = 256;
    DWORD valSz = sizeof(fontPath);
    DWORD type = 0;
    DWORD idx = 0;
    while (RegEnumValueW(hKey, idx++, valueName, &valNameSz,
                         nullptr, &type, (BYTE*)fontPath, &valSz) == ERROR_SUCCESS) {
      if (wcsstr(valueName, fontName)) { found = true; break; }
      valNameSz = 256; valSz = sizeof(fontPath);
    }
    RegCloseKey(hKey);
  }

  if (!found) {
    wchar_t tmpPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpPath);
    wcscat_s(tmpPath, L"NaraInter.ttf");
    if (GetFileAttributesW(tmpPath) == INVALID_FILE_ATTRIBUTES) {
      URLDownloadToFileW(nullptr,
        L"https://github.com/rsms/inter/raw/master/docs/font-files/Inter-Regular.ttf",
        tmpPath, 0, nullptr);
    }
    if (GetFileAttributesW(tmpPath) != INVALID_FILE_ATTRIBUTES) {
      AddFontResourceW(tmpPath);
      fontName = L"Inter";
      found = true;
    }
  }

  if (!found) fontName = fallback;

  LOGFONTW lf = {};
  lf.lfHeight = -11;
  lf.lfWeight = FW_NORMAL;
  wcscpy_s(lf.lfFaceName, fontName);
  gToolbarFont = CreateFontIndirectW(&lf);

  LOGFONTW lfAddr = {};
  lfAddr.lfHeight = -13;
  lfAddr.lfWeight = FW_NORMAL;
  wcscpy_s(lfAddr.lfFaceName, fontName);
  gAddrFont = CreateFontIndirectW(&lfAddr);

  gTabFont = gToolbarFont;
}

static bool gToolbarReady = false;

static WNDPROC gOrigAddrProc = nullptr;
static LRESULT CALLBACK AddrEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == WM_CHAR && wParam == VK_RETURN) {
    if (gBrowser) {
      wchar_t url[1024];
      GetWindowTextW(hwnd, url, 1024);
      std::wstring ws(url);
      if (ws.find(L'.') != std::wstring::npos &&
          ws.find(L' ') == std::wstring::npos &&
          ws.find(L"://") == std::wstring::npos) {
        ws = L"https://" + ws;
      }
      gBrowser->Navigate(ws);
    }
    return 0;
  }
  return CallWindowProcW(gOrigAddrProc, hwnd, msg, wParam, lParam);
}

// -------------------------------------------------------------------
// Create toolbar parts
// -------------------------------------------------------------------
static void CreateToolbar(HWND parent) {
  if (!gToolbarFont) LoadToolbarFont();

  // Tab bar area (just an owner-draw area where we'll place tab buttons)
  // For simplicity, use actual button children

  // Navigation + address bar row (row 1)
  int y = kTabBarHeight + kBtnTop;
  int x = 6;

  auto navBtn = [&](int id, const wchar_t* text, int w) {
    HWND h = CreateWindowExW(0, L"BUTTON", text,
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      x, y, w, kBtnH, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    if (h && gToolbarFont) SendMessageW(h, WM_SETFONT, (WPARAM)gToolbarFont, TRUE);
    x += w + kBtnGap;
  };

  // Back, Forward, Reload
  navBtn(kIdBack,    L"\x25C0", kNavBtnW);   // ◀
  navBtn(kIdForward, L"\x25B6", kNavBtnW);   // ▶
  navBtn(kIdReload,  L"\x21BB", kNavBtnW);   // ↻

  // Address bar edit
  x += 4;
  int addrW = 580;
  RECT rc;
  GetClientRect(parent, &rc);
  int minAddr = 200;
  addrW = (rc.right - x - kNavBtnW - kBtnGap - 4);
  if (addrW < minAddr) addrW = minAddr;

  HWND addrEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
    x, y, addrW, kBtnH, parent, (HMENU)(INT_PTR)kIdAddrEdit, nullptr, nullptr);
  if (addrEdit) {
    if (gAddrFont) SendMessageW(addrEdit, WM_SETFONT, (WPARAM)gAddrFont, TRUE);
    // Subclass for ENTER key
    gOrigAddrProc = (WNDPROC)SetWindowLongPtrW(addrEdit, GWLP_WNDPROC, (LONG_PTR)AddrEditProc);
    // store HWND for later - gBrowser might not be set yet
    SetWindowLongPtrW(addrEdit, GWLP_USERDATA, (LONG_PTR)addrEdit);
  }
  x += addrW + kBtnGap;

  // Go button
  navBtn(kIdGoAddr, T(S_GO), kNavBtnW + 6);

  // Search in page
  navBtn(kIdSearch, T(S_SEARCH), kNavBtnW);

  // Settings
  navBtn(kIdSettingsButton, T(S_SETTINGS), kNavBtnW);

  // Site buttons row (row 2)
  y = kTabBarHeight + kToolbarHeight + kBtnTop;
  x = 6;

  auto siteBtn = [&](int id, const wchar_t* text) {
    HWND h = CreateWindowExW(0, L"BUTTON", text,
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      x, y, kBtnW, kBtnH, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    if (h && gToolbarFont) SendMessageW(h, WM_SETFONT, (WPARAM)gToolbarFont, TRUE);
    x += kBtnW + kBtnGap;
  };

  siteBtn(kIdGoogleButton,    T(S_GOOGLE));
  siteBtn(kIdPinterestButton, T(S_PINTEREST));
  siteBtn(kIdRobloxButton,    T(S_ROBLOX));
  siteBtn(kIdDiscordButton,   T(S_DISCORD));
  siteBtn(kIdSpotifyButton,   T(S_SPOTIFY));
  siteBtn(kIdDownloads,       T(S_DOWNLOADS));
  siteBtn(kIdSiteDark,        T(S_DARK));
  x += 8;
  siteBtn(kIdImportCookies,   T(S_IMPORT));
  siteBtn(kIdExportCookies,   T(S_EXPORT));

  // Tab bar
  // Create tab buttons at the very top
  int tabY = 0;
  HWND newTabBtn = CreateWindowExW(0, L"BUTTON", T(S_NEW_TAB),
    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
    0, tabY, 28, kTabBarHeight, parent, (HMENU)(INT_PTR)kIdNewTab, nullptr, nullptr);
  if (newTabBtn && gTabFont) SendMessageW(newTabBtn, WM_SETFONT, (WPARAM)gTabFont, TRUE);
}

// Recreate tab buttons
static void UpdateTabBar(HWND hwnd) {
  if (!gBrowser) return;

  // Destroy old tab buttons (keep + button and toolbar)
  HWND child = GetWindow(hwnd, GW_CHILD);
  while (child) {
    HWND next = GetWindow(child, GW_HWNDNEXT);
    int id = GetDlgCtrlID(child);
    if (id == kIdNewTab || id == 0 || (id >= 100 && id < 500)) {
      child = next;
      continue;
    }
    if ((id >= kIdTabBase && id < kIdTabBase + 100) ||
        (id >= kIdTabCloseBase && id < kIdTabCloseBase + 100)) {
      DestroyWindow(child);
    }
    child = next;
  }

  int nTabs = gBrowser->TabCount();
  int tabW = 140;
  int x = 2;

  // Position + button first, then create tab buttons
  HWND newBtn = GetDlgItem(hwnd, kIdNewTab);
  if (newBtn) {
    SetWindowPos(newBtn, nullptr, x, 0, 28, kTabBarHeight, SWP_NOZORDER);
    x += 30;
  }

  int activeTab = gBrowser->ActiveTab();
  for (int i = 0; i < nTabs && i < 20; i++) {
    std::wstring label = gBrowser->TabTitle(i);
    if (label.empty()) label = std::wstring(T(S_TAB_PREFIX)) + std::to_wstring(i + 1);
    if (label.size() > 12) { label = label.substr(0, 10) + L".."; }

    // Tab label button
    int btnW = tabW - 18;
    HWND hBtn = CreateWindowExW(0, L"BUTTON", label.c_str(),
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      x, 0, btnW, kTabBarHeight, hwnd, (HMENU)(INT_PTR)(kIdTabBase + i),
      nullptr, nullptr);
    if (hBtn && gTabFont) {
      SendMessageW(hBtn, WM_SETFONT, (WPARAM)gTabFont, TRUE);
      if (i == activeTab) {
        SetWindowLongPtrW(hBtn, GWL_STYLE, GetWindowLongPtrW(hBtn, GWL_STYLE) | BS_DEFPUSHBUTTON);
        InvalidateRect(hBtn, nullptr, TRUE);
      }
    }

    // Close X button
    HWND hClose = CreateWindowExW(0, L"BUTTON", L"\xD7",
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      x + btnW, 0, 18, kTabBarHeight, hwnd, (HMENU)(INT_PTR)(kIdTabCloseBase + i),
      nullptr, nullptr);
    if (hClose && gTabFont) SendMessageW(hClose, WM_SETFONT, (WPARAM)gTabFont, TRUE);

    x += tabW + 2;
  }
}

// -------------------------------------------------------------------
// Resize children
// -------------------------------------------------------------------
static void ResizeChildren(HWND hwnd) {
  if (!gBrowser) return;

  RECT rc;
  GetClientRect(hwnd, &rc);

  // Tab bar is at the top
  // Find the + button
  HWND child = GetWindow(hwnd, GW_CHILD);
  while (child) {
    int id = GetDlgCtrlID(child);
    if (id == kIdNewTab) {
      SetWindowPos(child, nullptr, 0, 0, 28, kTabBarHeight, SWP_NOZORDER);
    }
    child = GetWindow(child, GW_HWNDNEXT);
  }

  // Resize address bar dynamically
  if (gBrowser->addrEdit_) {
    RECT editRc;
    GetWindowRect(gBrowser->addrEdit_, &editRc);
    HWND goBtn = GetDlgItem(hwnd, kIdGoAddr);
    HWND searchBtn = GetDlgItem(hwnd, kIdSearch);
    HWND settingsBtn = GetDlgItem(hwnd, kIdSettingsButton);
    HWND backBtn = GetDlgItem(hwnd, kIdBack);
    HWND fwdBtn = GetDlgItem(hwnd, kIdForward);
    HWND reloadBtn = GetDlgItem(hwnd, kIdReload);

    // Calculate available width
    int x = 6;
    if (backBtn) {
      RECT btnRc;
      GetWindowRect(backBtn, &btnRc);
      // nav buttons start at x=6, each is kNavBtnW + kBtnGap
    }
    int navBtnsWidth = 3 * (kNavBtnW + kBtnGap) + 4; // back+fwd+reload + gap
    int rightBtnsWidth = (kNavBtnW + kBtnGap) * 2 + (kNavBtnW + 6); // search + settings + go
    int addrWidth = rc.right - navBtnsWidth - rightBtnsWidth - 6 - 6;
    if (addrWidth < 200) addrWidth = 200;

    int addrY = kTabBarHeight + kBtnTop;
    SetWindowPos(gBrowser->addrEdit_, nullptr,
      navBtnsWidth, addrY, addrWidth, kBtnH, SWP_NOZORDER);

    if (goBtn) SetWindowPos(goBtn, nullptr,
      navBtnsWidth + addrWidth + kBtnGap, addrY, kNavBtnW + 6, kBtnH, SWP_NOZORDER);
    if (searchBtn) SetWindowPos(searchBtn, nullptr,
      navBtnsWidth + addrWidth + kBtnGap + kNavBtnW + 6 + kBtnGap, addrY, kNavBtnW, kBtnH, SWP_NOZORDER);
    if (settingsBtn) SetWindowPos(settingsBtn, nullptr,
      navBtnsWidth + addrWidth + kBtnGap + kNavBtnW + 6 + kBtnGap + kNavBtnW + kBtnGap, addrY, kNavBtnW, kBtnH, SWP_NOZORDER);
  }

  // Resize webview
  gBrowser->Resize();
}

// -------------------------------------------------------------------
// Brush management
// -------------------------------------------------------------------
static void UpdateToolbarBrush(bool dark) {
  if (gToolbarBrush) DeleteObject(gToolbarBrush);
  gToolbarBrush = CreateSolidBrush(dark ? RGB(50, 50, 55) : RGB(240, 242, 245));
  if (gSiteRowBrush) DeleteObject(gSiteRowBrush);
  gSiteRowBrush = CreateSolidBrush(dark ? RGB(45, 45, 50) : RGB(232, 234, 237));
  InvalidateRect(gMainHwnd, nullptr, TRUE);
}



// -------------------------------------------------------------------
// Update toolbar texts after language switch
// -------------------------------------------------------------------
static void UpdateToolbarTexts(HWND hwnd) {
  auto setText = [&](int id) {
    HWND h = GetDlgItem(hwnd, id);
    if (h) {
      const wchar_t* text = L"";
      switch (id) {
        case kIdGoAddr:        text = T(S_GO); break;
        case kIdSearch:        text = T(S_SEARCH); break;
        case kIdSettingsButton: text = T(S_SETTINGS); break;
        case kIdGoogleButton:  text = T(S_GOOGLE); break;
        case kIdPinterestButton: text = T(S_PINTEREST); break;
        case kIdRobloxButton:  text = T(S_ROBLOX); break;
        case kIdDiscordButton: text = T(S_DISCORD); break;
        case kIdSpotifyButton: text = T(S_SPOTIFY); break;
        case kIdDownloads:     text = T(S_DOWNLOADS); break;
        case kIdSiteDark:      text = T(S_DARK); break;
        case kIdImportCookies: text = T(S_IMPORT); break;
        case kIdExportCookies: text = T(S_EXPORT); break;
        case kIdNewTab:        text = T(S_NEW_TAB); break;
      }
      SetWindowTextW(h, text);
    }
  };
  setText(kIdGoAddr);
  setText(kIdSearch);
  setText(kIdSettingsButton);
  setText(kIdGoogleButton);
  setText(kIdPinterestButton);
  setText(kIdRobloxButton);
  setText(kIdDiscordButton);
  setText(kIdSpotifyButton);
  setText(kIdDownloads);
  setText(kIdSiteDark);
  setText(kIdImportCookies);
  setText(kIdExportCookies);
  setText(kIdNewTab);
  UpdateTabBar(hwnd);
}

// ===================================================================
// Window procedure
// ===================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      UpdateToolbarBrush(false);
      CreateToolbar(hwnd);
      break;

    case WM_ERASEBKGND: {
      HDC dc = (HDC)wParam;
      RECT rc;
      GetClientRect(hwnd, &rc);

      // Tab bar area
      RECT tabRc = {0, 0, rc.right, kTabBarHeight};
      FillRect(dc, &tabRc, (HBRUSH)GetStockObject(WHITE_BRUSH));

      RECT navRc = {0, kTabBarHeight, rc.right, kTabBarHeight + kToolbarHeight};
      FillRect(dc, &navRc, gToolbarBrush ? gToolbarBrush : (HBRUSH)GetStockObject(WHITE_BRUSH));

      RECT siteRc = {0, kTabBarHeight + kToolbarHeight, rc.right, kTabBarHeight + kToolbarHeight + kSiteRowHeight};
      HBRUSH siteBrush = gSiteRowBrush ? gSiteRowBrush : gToolbarBrush;
      FillRect(dc, &siteRc, siteBrush ? siteBrush : (HBRUSH)GetStockObject(WHITE_BRUSH));

      RECT webRc = {0, kTabBarHeight + kToolbarHeight + kSiteRowHeight, rc.right, rc.bottom};
      FillRect(dc, &webRc, (HBRUSH)GetStockObject(WHITE_BRUSH));
      return TRUE;
    }

    case WM_SIZE:
      UpdateTabBar(hwnd);
      ResizeChildren(hwnd);
      break;

    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        // Navigation
        case kIdBack:
          if (gBrowser) gBrowser->GoBack();
          break;
        case kIdForward:
          if (gBrowser) gBrowser->GoForward();
          break;
        case kIdReload:
          if (gBrowser) gBrowser->Reload();
          break;
        case kIdSearch:
          if (gBrowser) gBrowser->SearchInPage();
          break;

        // Go button
        case kIdGoAddr: {
          if (gBrowser && gBrowser->addrEdit_) {
            wchar_t url[1024];
            GetWindowTextW(gBrowser->addrEdit_, url, 1024);
            std::wstring ws(url);
            if (ws.find(L'.') != std::wstring::npos &&
                ws.find(L' ') == std::wstring::npos &&
                ws.find(L"://") == std::wstring::npos) {
              ws = L"https://" + ws;
            }
            gBrowser->Navigate(ws);
          }
          break;
        }

        // New tab
        case kIdNewTab:
          if (gBrowser) {
            gBrowser->NewTab(L"https://www.google.com");
            UpdateTabBar(hwnd);
          }
          break;
        default:
          // Tab selection
          if (LOWORD(wParam) >= kIdTabBase && LOWORD(wParam) < kIdTabBase + 100) {
            if (gBrowser) {
              int tabIdx = LOWORD(wParam) - kIdTabBase;
              if (tabIdx >= 0 && tabIdx < gBrowser->TabCount()) {
                gBrowser->SwitchTab(tabIdx);
                UpdateTabBar(hwnd);
              }
            }
          }
          // Tab close
          if (LOWORD(wParam) >= kIdTabCloseBase && LOWORD(wParam) < kIdTabCloseBase + 100) {
            if (gBrowser) {
              int tabIdx = LOWORD(wParam) - kIdTabCloseBase;
              if (tabIdx >= 0 && tabIdx < gBrowser->TabCount()) {
                gBrowser->CloseTab(tabIdx);
                UpdateTabBar(hwnd);
              }
            }
          }
          break;

        // Site shortcuts
        case kIdGoogleButton:
          if (gBrowser) gBrowser->Navigate(L"https://www.google.com");
          break;
        case kIdPinterestButton:
          if (gBrowser) gBrowser->Navigate(L"https://www.pinterest.com");
          break;
        case kIdRobloxButton:
          if (gBrowser) gBrowser->Navigate(L"https://www.roblox.com");
          break;
        case kIdDiscordButton:
          if (gBrowser) gBrowser->Navigate(L"https://discord.com");
          break;
        case kIdSpotifyButton:
          if (gBrowser) gBrowser->Navigate(L"https://open.spotify.com");
          break;
        case kIdDownloads:
          if (gBrowser) gBrowser->ShowDownloads();
          break;
        case kIdSiteDark:
          if (gBrowser) gBrowser->ToggleSiteDark();
          break;
        case kIdImportCookies:
          if (gBrowser) gBrowser->ImportCookies();
          break;
        case kIdExportCookies:
          if (gBrowser) gBrowser->ExportCookies();
          break;
        case kIdAccelReload:
          if (gBrowser) gBrowser->Reload();
          break;

        case kIdSettingsButton: {
          HMENU menu = CreatePopupMenu();
          AppendMenuW(menu, MF_STRING, kIdToggleTheme,
                      gBrowser && gBrowser->IsDark() ? T(S_LIGHT_MODE) : T(S_DARK_MODE));
          HMENU langMenu = CreatePopupMenu();
          AppendMenuW(langMenu, MF_STRING, kIdLangEnglish, T(S_ENGLISH));
          AppendMenuW(langMenu, MF_STRING, kIdLangDutch, T(S_NEDERLANDS));
          AppendMenuW(langMenu, MF_STRING, kIdLangPolish, T(S_POLSKI));
          AppendMenuW(menu, MF_POPUP, (UINT_PTR)langMenu, T(S_LANGUAGE));
          AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
          AppendMenuW(menu, MF_STRING, kIdCredits, T(S_CREDITS));
          POINT pt;
          GetCursorPos(&pt);
          SetForegroundWindow(hwnd);
          TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                         pt.x, pt.y, 0, hwnd, nullptr);
          DestroyMenu(langMenu);
          DestroyMenu(menu);
          break;
        }
        case kIdToggleTheme:
          if (gBrowser) {
            gBrowser->ToggleTheme();
            UpdateToolbarBrush(gBrowser->IsDark());
          }
          break;
        case kIdLangEnglish:
          gCurrentLang = Lang::English;
          UpdateToolbarTexts(hwnd);
          break;
        case kIdLangDutch:
          gCurrentLang = Lang::Nederlands;
          UpdateToolbarTexts(hwnd);
          break;
        case kIdLangPolish:
          gCurrentLang = Lang::Polski;
          UpdateToolbarTexts(hwnd);
          break;
        case kIdCredits: {
          MessageBoxW(hwnd, T(S_CREDITS_TEXT), T(S_CREDITS_TITLE), MB_OK);
          break;
        }
      }
      break;

    case kMsgCreateTab:
      if (gBrowser) {
        if (lParam == 1) {
          MessageBoxW(nullptr, T(S_WV2_NOT_FOUND), T(S_ERROR), MB_OK);
        } else {
          gBrowser->NewTab(L"https://www.google.com");
          UpdateTabBar(hwnd);
        }
      }
      break;

    case WM_DESTROY:
      if (gToolbarBrush) DeleteObject(gToolbarBrush);
      if (gSiteRowBrush) DeleteObject(gSiteRowBrush);
      PostQuitMessage(0);
      break;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ===================================================================
// Main window creation
// ===================================================================
HWND CreateMainWindow(HINSTANCE hInstance) {
  const wchar_t kClassName[] = L"NaraWindow";

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = kClassName;

  RegisterClassExW(&wc);

  DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
  RECT rc = {0, 0, kWindowWidth, kWindowHeight};
  AdjustWindowRect(&rc, style, FALSE);

  HWND hwnd = CreateWindowExW(
      0, kClassName, L"Nara",
      style, CW_USEDEFAULT, CW_USEDEFAULT,
      rc.right - rc.left, rc.bottom - rc.top,
      nullptr, nullptr, hInstance, nullptr);

  gMainHwnd = hwnd;

  HICON hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
  if (hIcon) {
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
  }

  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);
  return hwnd;
}

// ===================================================================
// Entry point
// ===================================================================
int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE, LPSTR, int) {
  HWND hwnd = CreateMainWindow(hInstance);

  ACCEL accel[] = {
    { FCONTROL, 'R', kIdAccelReload },
  };
  HACCEL hAccel = CreateAcceleratorTableW(accel, 1);

  BrowserWindow browser;
  gBrowser = &browser;
  // Find the address bar edit control and store it
  gBrowser->addrEdit_ = GetDlgItem(hwnd, kIdAddrEdit);
  browser.Initialize(hwnd);

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    if (!TranslateAcceleratorW(hwnd, hAccel, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  if (hAccel) DestroyAcceleratorTable(hAccel);
  return (int)msg.wParam;
}
