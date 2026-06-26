#include "main.h"
#include <shellapi.h>

HWND gMainHwnd = nullptr;
bool gIsDark = false;
static HBRUSH gToolbarBrush = nullptr;

static void CreateToolbar(HWND parent) {
  int x = 6, y = kBtnTop;
  auto btn = [&](int id, const wchar_t* text) {
    CreateWindowExW(0, L"BUTTON", text,
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      x, y, kBtnW, kBtnH, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    x += kBtnW + kBtnGap;
  };
  btn(kIdGoogleButton,    L"Google");
  btn(kIdPinterestButton, L"Pinterest");
  btn(kIdRobloxButton,    L"Roblox");
  btn(kIdDiscordButton,   L"Discord");
  x += 24;
  btn(kIdSettingsButton,  L"Settings");
}

static void UpdateToolbarBrush(bool dark) {
  if (gToolbarBrush) DeleteObject(gToolbarBrush);
  gToolbarBrush = CreateSolidBrush(dark ? RGB(45, 45, 45) : GetSysColor(COLOR_MENU));
  InvalidateRect(gMainHwnd, nullptr, TRUE);
}

void NavigateFirefox(const wchar_t* url) {
  ShellExecuteW(nullptr, L"open", L"firefox.exe", url, nullptr, SW_SHOW);
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
      RECT clientRc = rc;
      clientRc.top = kToolbarHeight;
      FillRect(dc, &clientRc, (HBRUSH)GetStockObject(WHITE_BRUSH));
      return TRUE;
    }

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(hwnd, &ps);
      RECT rc;
      GetClientRect(hwnd, &rc);
      rc.top = kToolbarHeight;
      SetBkMode(dc, TRANSPARENT);
      SetTextColor(dc, gIsDark ? RGB(200,200,200) : RGB(100,100,100));
      HGDIOBJ oldFont = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
      const wchar_t* msg = L"Klik een knop om Firefox te openen";
      DrawTextW(dc, msg, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      SelectObject(dc, oldFont);
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case kIdGoogleButton:     NavigateFirefox(L"https://www.google.com"); break;
        case kIdPinterestButton:  NavigateFirefox(L"https://www.pinterest.com"); break;
        case kIdRobloxButton:     NavigateFirefox(L"https://www.roblox.com"); break;
        case kIdDiscordButton:    NavigateFirefox(L"https://discord.com"); break;
        case kIdAccelReload:      NavigateFirefox(L""); break;
        case kIdSettingsButton: {
          HMENU menu = CreatePopupMenu();
          AppendMenuW(menu, MF_STRING, kIdToggleTheme, gIsDark ? L"Light Mode" : L"Dark Mode");
          POINT pt;
          GetCursorPos(&pt);
          SetForegroundWindow(hwnd);
          TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
          DestroyMenu(menu);
          break;
        }
        case kIdToggleTheme:
          gIsDark = !gIsDark;
          UpdateToolbarBrush(gIsDark);
          InvalidateRect(hwnd, nullptr, TRUE);
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

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
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

  DWORD style = WS_OVERLAPPEDWINDOW;
  RECT rc = {0, 0, kWindowWidth, kWindowHeight};
  AdjustWindowRect(&rc, style, FALSE);

  HWND hwnd = CreateWindowExW(0, kClassName, L"nara",
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

  ACCEL accel = { FCONTROL, 'R', kIdAccelReload };
  HACCEL hAccel = CreateAcceleratorTableW(&accel, 1);

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
