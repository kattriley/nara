#include "main.h"
#include "browser_window.h"

HWND gMainHwnd = nullptr;
BrowserWindow* gBrowser = nullptr;

static HBRUSH gToolbarBrush = nullptr;

static void CreateToolbar(HWND parent) {
  int x = 6, y = kBtnTop;
  auto btn = [&](int id, const wchar_t* text) {
    CreateWindowExW(0, L"BUTTON", text,
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      x, y, kBtnW, kBtnH, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    x += kBtnW + kBtnGap;
  };
  btn(kIdGoogleButton,    L"\xD83D\xDD0D Google");
  btn(kIdPinterestButton, L"\xD83D\xDCCC Pinterest");
  btn(kIdRobloxButton,    L"\x2B50 Roblox");
  btn(kIdDiscordButton,   L"\xD83C\xDFAE Discord");
  btn(kIdSpotifyButton,   L"\xD83C\xDFB5 Spotify");
  x += 8;
  btn(kIdImportCookies,   L"\xD83D\xDCE5 Import");
  btn(kIdExportCookies,   L"\xD83D\xDCE4 Export");
  btn(kIdSettingsButton,  L"\x2699 Settings");
}

static void ResizeChildren(HWND hwnd) {
  if (!gBrowser) return;
  RECT rc;
  GetClientRect(hwnd, &rc);
  rc.top += kToolbarHeight;
  gBrowser->Resize();
}

static void UpdateToolbarBrush(bool dark) {
  if (gToolbarBrush) DeleteObject(gToolbarBrush);
  gToolbarBrush = CreateSolidBrush(dark ? RGB(50, 50, 55) : RGB(240, 242, 245));
  InvalidateRect(gMainHwnd, nullptr, TRUE);
}

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
      RECT toolbarRc = rc;
      toolbarRc.bottom = kToolbarHeight;
      FillRect(dc, &toolbarRc, gToolbarBrush);
      RECT webRc = rc;
      webRc.top = kToolbarHeight;
      FillRect(dc, &webRc, (HBRUSH)GetStockObject(WHITE_BRUSH));
      return TRUE;
    }

    case WM_SIZE:
      ResizeChildren(hwnd);
      break;

    case WM_COMMAND:
      switch (LOWORD(wParam)) {
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
                      gBrowser && gBrowser->IsDark() ? L"Light Mode" : L"Dark Mode");
          POINT pt;
          GetCursorPos(&pt);
          SetForegroundWindow(hwnd);
          TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                         pt.x, pt.y, 0, hwnd, nullptr);
          DestroyMenu(menu);
          break;
        }
        case kIdToggleTheme:
          if (gBrowser) {
            gBrowser->ToggleTheme();
            UpdateToolbarBrush(gBrowser->IsDark());
          }
          break;

      }
      break;

    case WM_DESTROY:
      if (gToolbarBrush) DeleteObject(gToolbarBrush);
      PostQuitMessage(0);
      break;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

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

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE, LPSTR, int) {
  HWND hwnd = CreateMainWindow(hInstance);

  ACCEL accel = { FCONTROL, 'R', kIdAccelReload };
  HACCEL hAccel = CreateAcceleratorTableW(&accel, 1);

  BrowserWindow browser;
  gBrowser = &browser;
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
