#include "browser_window.h"
#include <fstream>
#include <sstream>
#include <vector>

// -------------------------------------------------------------------
// UTF-8 / Wide helpers
// -------------------------------------------------------------------
static std::string WideToUTF8(const wchar_t* w) {
  if (!w) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  std::string s(static_cast<size_t>(n) - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
  return s;
}

static std::wstring UTF8ToWide(const std::string& s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  std::wstring w(static_cast<size_t>(n) - 1, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
  return w;
}

static std::wstring FormatDouble(double d) {
  wchar_t buf[64];
  swprintf_s(buf, L"%.15g", d);
  return buf;
}

static double ParseDouble(const std::wstring& s) {
  return wcstod(s.c_str(), nullptr);
}

// -------------------------------------------------------------------
// Base64
// -------------------------------------------------------------------
static const char kBase64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string Base64Encode(const std::string& in) {
  std::string out;
  int val = 0, valb = -6;
  for (unsigned char c : in) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      out.push_back(kBase64[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6)
    out.push_back(kBase64[((val << 8) >> (valb + 8)) & 0x3F]);
  while (out.size() % 4)
    out.push_back('=');
  return out;
}

static std::string Base64Decode(const std::string& in) {
  std::vector<int> T(256, -1);
  for (int i = 0; i < 64; i++)
    T[static_cast<unsigned char>(kBase64[i])] = i;
  std::string out;
  int val = 0, valb = -8;
  for (unsigned char c : in) {
    if (c == '=') break;
    if (T[c] == -1) continue;
    val = (val << 6) + T[c];
    valb += 6;
    if (valb >= 0) {
      out.push_back(static_cast<char>((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return out;
}

// -------------------------------------------------------------------
// Cookie path
// -------------------------------------------------------------------
static std::wstring GetCookiePath() {
  wchar_t buf[MAX_PATH];
  GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
  wcscat_s(buf, L"\\Nara");
  CreateDirectoryW(buf, nullptr);
  wcscat_s(buf, L"\\cookies.txt");
  return buf;
}

// -------------------------------------------------------------------
// Password path
// -------------------------------------------------------------------
static std::wstring GetPasswordPath() {
  wchar_t buf[MAX_PATH];
  GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
  wcscat_s(buf, L"\\Nara");
  CreateDirectoryW(buf, nullptr);
  wcscat_s(buf, L"\\passwords.txt");
  return buf;
}

// -------------------------------------------------------------------
// GetCookies callback for export
// -------------------------------------------------------------------
struct ExportHandler : ICoreWebView2GetCookiesCompletedHandler {
  std::wstring filePath;
  ExportHandler(const std::wstring& fp) : filePath(fp) {}
  STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override { *ppv = nullptr; return E_NOINTERFACE; }
  STDMETHOD_(ULONG, AddRef)() override { return 2; }
  STDMETHOD_(ULONG, Release)() override { return 1; }
  STDMETHOD(Invoke)(HRESULT errorCode, ICoreWebView2CookieList* result) override {
    if (FAILED(errorCode) || !result) return errorCode;
    UINT32 count = 0;
    result->get_Count(&count);

    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return S_OK;

    std::string buf;
    for (UINT32 i = 0; i < count; ++i) {
      ICoreWebView2Cookie* c = nullptr;
      result->GetValueAtIndex(i, &c);
      if (!c) continue;

      LPWSTR name = nullptr, value = nullptr, domain = nullptr, path = nullptr;
      double expires = 0;
      BOOL isSecure = FALSE, isHttpOnly = FALSE, isSession = FALSE;
      COREWEBVIEW2_COOKIE_SAME_SITE_KIND sameSite = COREWEBVIEW2_COOKIE_SAME_SITE_KIND_NONE;

      c->get_Name(&name);
      c->get_Value(&value);
      c->get_Domain(&domain);
      c->get_Path(&path);
      c->get_Expires(&expires);
      c->get_IsSecure(&isSecure);
      c->get_IsHttpOnly(&isHttpOnly);
      c->get_IsSession(&isSession);
      c->get_SameSite(&sameSite);

      buf += WideToUTF8(name) + '\x01';
      buf += WideToUTF8(value) + '\x01';
      buf += WideToUTF8(domain) + '\x01';
      buf += WideToUTF8(path) + '\x01';
      buf += WideToUTF8(FormatDouble(expires).c_str()) + '\x01';
      buf += (isSecure ? "1" : "0") + std::string("\x01");
      buf += (isHttpOnly ? "1" : "0") + std::string("\x01");
      buf += std::to_string(static_cast<int>(sameSite)) + '\x01';
      buf += (isSession ? "1" : "0") + std::string("\n");

      CoTaskMemFree(name);
      CoTaskMemFree(value);
      CoTaskMemFree(domain);
      CoTaskMemFree(path);
      c->Release();
    }

    DWORD written = 0;
    WriteFile(hFile, buf.data(), static_cast<DWORD>(buf.size()), &written, nullptr);
    CloseHandle(hFile);
    return S_OK;
  }
};

// -------------------------------------------------------------------
// BrowserWindow
// -------------------------------------------------------------------
struct EnvHandler : ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
  BrowserWindow* bw;
  HWND parent;
  EnvHandler(BrowserWindow* b, HWND p) : bw(b), parent(p) {}
  STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override { *ppv = nullptr; return E_NOINTERFACE; }
  STDMETHOD_(ULONG, AddRef)() override { return 2; }
  STDMETHOD_(ULONG, Release)() override { return 1; }
  STDMETHOD(Invoke)(HRESULT hr, ICoreWebView2Environment* env) override;
};

struct CtrlHandler : ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
  BrowserWindow* bw;
  CtrlHandler(BrowserWindow* b) : bw(b) {}
  STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override { *ppv = nullptr; return E_NOINTERFACE; }
  STDMETHOD_(ULONG, AddRef)() override { return 2; }
  STDMETHOD_(ULONG, Release)() override { return 1; }
  STDMETHOD(Invoke)(HRESULT hr, ICoreWebView2Controller* controller) override;
};

HRESULT EnvHandler::Invoke(HRESULT hr, ICoreWebView2Environment* env) {
  if (FAILED(hr) || !env) {
    MessageBoxA(nullptr,
      "WebView2 runtime niet gevonden.\n"
      "Installeer van: https://go.microsoft.com/fwlink/p/?LinkId=2124703",
      "Fout", MB_OK);
    return hr;
  }
  bw->env_ = env;
  bw->env_->AddRef();
  env->CreateCoreWebView2Controller(parent, new CtrlHandler(bw));
  return S_OK;
}

HRESULT CtrlHandler::Invoke(HRESULT hr, ICoreWebView2Controller* controller) {
  if (FAILED(hr) || !controller) {
    MessageBoxA(nullptr, "Kon WebView2 controller niet maken.", "Fout", MB_OK);
    return hr;
  }
  bw->controller_ = controller;
  bw->controller_->AddRef();
  controller->get_CoreWebView2(&bw->webview_);

  bw->Navigate(L"https://www.google.com");
  bw->Resize();
  return S_OK;
}

BrowserWindow::BrowserWindow()
  : env_(nullptr), controller_(nullptr), webview_(nullptr) {}

BrowserWindow::~BrowserWindow() {
  if (controller_) controller_->Release();
  if (webview_) webview_->Release();
  if (env_) env_->Release();
}

bool BrowserWindow::Initialize(HWND parent) {
  EnvHandler* handler = new EnvHandler(this, parent);
  HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, handler);
  return SUCCEEDED(hr);
}

void BrowserWindow::Navigate(const std::wstring& url) {
  if (webview_) webview_->Navigate(url.c_str());
}

void BrowserWindow::Navigate(const std::string& url) {
  if (!webview_) return;
  int len = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
  std::wstring wurl(static_cast<size_t>(len) - 1, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &wurl[0], len);
  webview_->Navigate(wurl.c_str());
}

void BrowserWindow::GoBack()    { if (webview_) webview_->GoBack(); }
void BrowserWindow::GoForward() { if (webview_) webview_->GoForward(); }
void BrowserWindow::Reload()    { if (webview_) webview_->Reload(); }

void BrowserWindow::Resize() {
  if (!controller_) return;
  HWND parent = nullptr;
  controller_->get_ParentWindow(&parent);
  RECT rc;
  GetClientRect(parent, &rc);
  rc.top += 38;
  controller_->put_Bounds(rc);
}

// -------------------------------------------------------------------
// Cookie export
// -------------------------------------------------------------------
void BrowserWindow::ExportCookies() {
  if (!webview_) return;
  ICoreWebView2_2* wv2 = nullptr;
  HRESULT hr = webview_->QueryInterface(IID_ICoreWebView2_2, (void**)&wv2);
  if (FAILED(hr) || !wv2) {
    MessageBoxA(nullptr, "Kan CookieManager niet openen.", "Fout", MB_OK);
    return;
  }
  ICoreWebView2CookieManager* mgr = nullptr;
  wv2->get_CookieManager(&mgr);
  wv2->Release();
  if (!mgr) return;

  mgr->GetCookies(L"", new ExportHandler(GetCookiePath()));
  mgr->Release();
  MessageBoxA(nullptr, "Cookies geexporteerd naar cookies.txt", "Export", MB_OK);
}

// -------------------------------------------------------------------
// Cookie import
// -------------------------------------------------------------------
void BrowserWindow::ImportCookies() {
  if (!webview_) return;
  ICoreWebView2_2* wv2 = nullptr;
  HRESULT hr = webview_->QueryInterface(IID_ICoreWebView2_2, (void**)&wv2);
  if (FAILED(hr) || !wv2) {
    MessageBoxA(nullptr, "Kan CookieManager niet openen.", "Fout", MB_OK);
    return;
  }
  ICoreWebView2CookieManager* mgr = nullptr;
  wv2->get_CookieManager(&mgr);
  wv2->Release();
  if (!mgr) return;

  std::wstring path = GetCookiePath();
  HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hFile == INVALID_HANDLE_VALUE) {
    MessageBoxA(nullptr, "Geen cookies.txt gevonden.", "Import", MB_OK);
    mgr->Release();
    return;
  }

  DWORD size = GetFileSize(hFile, nullptr);
  std::string data(static_cast<size_t>(size), '\0');
  DWORD read = 0;
  ReadFile(hFile, &data[0], size, &read, nullptr);
  CloseHandle(hFile);

  if (data.empty() || data.back() != '\n') data.push_back('\n');

  size_t pos = 0;
  UINT32 imported = 0;
  while (pos < data.size()) {
    size_t nl = data.find('\n', pos);
    if (nl == std::string::npos) break;
    std::string line = data.substr(pos, nl - pos);
    pos = nl + 1;
    if (line.empty()) continue;

    std::vector<std::string> fields;
    size_t start = 0;
    while (start < line.size()) {
      size_t sep = line.find('\x01', start);
      if (sep == std::string::npos) {
        fields.push_back(line.substr(start));
        break;
      }
      fields.push_back(line.substr(start, sep - start));
      start = sep + 1;
    }

    if (fields.size() < 9) continue;

    ICoreWebView2Cookie* c = nullptr;
    HRESULT hr2 = mgr->CreateCookie(
        UTF8ToWide(fields[0]).c_str(),
        UTF8ToWide(fields[1]).c_str(),
        UTF8ToWide(fields[2]).c_str(),
        UTF8ToWide(fields[3]).c_str(),
        &c);
    if (FAILED(hr2) || !c) continue;

    c->put_IsSecure(fields[5] == "1" ? TRUE : FALSE);
    c->put_IsHttpOnly(fields[6] == "1" ? TRUE : FALSE);
    c->put_SameSite(static_cast<COREWEBVIEW2_COOKIE_SAME_SITE_KIND>(std::stoi(fields[7])));
    c->put_Expires(ParseDouble(UTF8ToWide(fields[4])));

    mgr->AddOrUpdateCookie(c);
    c->Release();
    ++imported;
  }

  mgr->Release();

  char msg[64];
  snprintf(msg, sizeof(msg), "%u cookies geimporteerd.", imported);
  MessageBoxA(nullptr, msg, "Import", MB_OK);
}

// -------------------------------------------------------------------
// Theme toggle
// -------------------------------------------------------------------
void BrowserWindow::ToggleTheme() {
  isDark_ = !isDark_;
  if (!webview_ || !controller_) return;

  ICoreWebView2Controller2* ctrl2 = nullptr;
  if (SUCCEEDED(controller_->QueryInterface(IID_ICoreWebView2Controller2, (void**)&ctrl2)) && ctrl2) {
    COREWEBVIEW2_COLOR bg;
    if (isDark_) {
      bg.R = 30; bg.G = 30; bg.B = 30; bg.A = 255;
    } else {
      bg.R = 255; bg.G = 255; bg.B = 255; bg.A = 255;
    }
    ctrl2->put_DefaultBackgroundColor(bg);
    ctrl2->Release();
  }

  ICoreWebView2_13* wv13 = nullptr;
  if (SUCCEEDED(webview_->QueryInterface(IID_ICoreWebView2_13, (void**)&wv13)) && wv13) {
    ICoreWebView2Profile* profile = nullptr;
    if (SUCCEEDED(wv13->get_Profile(&profile)) && profile) {
      profile->put_PreferredColorScheme(
          isDark_ ? COREWEBVIEW2_PREFERRED_COLOR_SCHEME_DARK
                  : COREWEBVIEW2_PREFERRED_COLOR_SCHEME_LIGHT);
      profile->Release();
    }
    wv13->Release();
  }

  const wchar_t* script = isDark_
    ? LR"(document.documentElement.style.backgroundColor='#1e1e1e';
try {
  let s=document.createElement('style');
  s.id='__mydark';
  s.textContent='html{filter:invert(.88)hue-rotate(180deg)}img,video,canvas,svg{filter:invert(1)hue-rotate(180deg)}';
  document.head.appendChild(s);
}catch(e){})"
    : LR"(try{
  let s=document.getElementById('__mydark');
  if(s)s.remove();
  document.documentElement.style.backgroundColor='';
}catch(e){})";

  webview_->ExecuteScript(script, nullptr);
}

// -------------------------------------------------------------------
// Password Manager
// -------------------------------------------------------------------

// Extract hostname from a URL
static std::wstring ExtractDomain(const std::wstring& url) {
  size_t start = url.find(L"://");
  if (start == std::wstring::npos) start = 0;
  else start += 3;
  size_t end = url.find(L'/', start);
  if (end == std::wstring::npos) return url.substr(start);
  return url.substr(start, end - start);
}

// Password save handler (receives JSON result of JS execution)
struct PmSaveHandler : ICoreWebView2ExecuteScriptCompletedHandler {
  BrowserWindow* bw;
  std::wstring domain;
  PmSaveHandler(BrowserWindow* b, const std::wstring& d) : bw(b), domain(d) {}
  STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override { *ppv = nullptr; return E_NOINTERFACE; }
  STDMETHOD_(ULONG, AddRef)() override { return 2; }
  STDMETHOD_(ULONG, Release)() override { return 1; }
  STDMETHOD(Invoke)(HRESULT, LPCWSTR resultObjectAsJson) override {
    if (!resultObjectAsJson) return S_OK;
    std::string json = WideToUTF8(resultObjectAsJson);
    // resultObjectAsJson is a JSON string: e.g. "user|||pass"
    // Strip surrounding quotes
    if (json.size() < 2) return S_OK;
    std::string raw = json.substr(1, json.size() - 2);
    size_t sep = raw.find("|||");
    if (sep == std::string::npos || sep == 0 || sep == raw.size() - 3) {
      MessageBoxA(nullptr, "Geen inlogformulier gevonden op deze pagina.", "Wachtwoord", MB_OK);
      return S_OK;
    }
    std::string user = raw.substr(0, sep);
    std::string pass = raw.substr(sep + 3);
    if (user.empty() || pass.empty()) {
      MessageBoxA(nullptr, "Vul eerst gebruikersnaam en wachtwoord in.", "Wachtwoord", MB_OK);
      return S_OK;
    }

    std::wstring pwPath = GetPasswordPath();
    std::string domainA = WideToUTF8(domain.c_str());
    std::string encodedDomain = Base64Encode(domainA);
    std::string encodedUser = Base64Encode(user);
    std::string encodedPass = Base64Encode(pass);

    std::vector<std::string> lines;
    HANDLE hFile = CreateFileW(pwPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
      DWORD sz = GetFileSize(hFile, nullptr);
      if (sz > 0) {
        std::string existing(static_cast<size_t>(sz), '\0');
        DWORD rd = 0;
        ReadFile(hFile, &existing[0], sz, &rd, nullptr);
        size_t p = 0;
        while (p < existing.size()) {
          size_t nl = existing.find('\n', p);
          if (nl == std::string::npos) break;
          lines.push_back(existing.substr(p, nl - p));
          p = nl + 1;
        }
      }
      CloseHandle(hFile);
    }

    bool updated = false;
    std::string newLine = encodedDomain + '\x01' + encodedUser + '\x01' + encodedPass;
    for (auto& line : lines) {
      size_t firstSep = line.find('\x01');
      if (firstSep != std::string::npos && line.substr(0, firstSep) == encodedDomain) {
        line = newLine;
        updated = true;
        break;
      }
    }
    if (!updated) lines.push_back(newLine);

    hFile = CreateFileW(pwPath.c_str(), GENERIC_WRITE, 0, nullptr,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
      std::string out;
      for (const auto& line : lines) out += line + '\n';
      DWORD written = 0;
      WriteFile(hFile, out.data(), static_cast<DWORD>(out.size()), &written, nullptr);
      CloseHandle(hFile);
      MessageBoxA(nullptr, "Wachtwoord opgeslagen!", "Wachtwoord", MB_OK);
    }
    return S_OK;
  }
};

void BrowserWindow::SavePassword() {
  if (!webview_) return;
  LPWSTR src = nullptr;
  webview_->get_Source(&src);
  std::wstring domain = src ? ExtractDomain(src) : L"unknown";
  CoTaskMemFree(src);

  const wchar_t* script =
    L"(function(){"
    L"var f=document.querySelector('form');"
    L"if(!f)return '|||';"
    L"var inputs=f.querySelectorAll('input');"
    L"var user='',pass='';"
    L"for(var i=0;i<inputs.length;i++){"
    L"var inp=inputs[i];"
    L"if(inp.type==='password')pass=inp.value;"
    L"else if(inp.type==='email'||inp.type==='text'){"
    L"if(!user)user=inp.value;"
    L"}"
    L"}"
    L"return user+'|||'+pass;"
    L"})()";

  webview_->ExecuteScript(script, new PmSaveHandler(this, domain));
}

// Password autofill handler
struct PmAutofillHandler : ICoreWebView2ExecuteScriptCompletedHandler {
  BrowserWindow* bw;
  std::wstring user;
  std::wstring pass;
  PmAutofillHandler(BrowserWindow* b, const std::wstring& u, const std::wstring& p)
    : bw(b), user(u), pass(p) {}
  STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override { *ppv = nullptr; return E_NOINTERFACE; }
  STDMETHOD_(ULONG, AddRef)() override { return 2; }
  STDMETHOD_(ULONG, Release)() override { return 1; }
  STDMETHOD(Invoke)(HRESULT, LPCWSTR) override { return S_OK; }
};

void BrowserWindow::AutoFillPassword() {
  if (!webview_) return;
  LPWSTR src = nullptr;
  webview_->get_Source(&src);
  std::wstring domain = src ? ExtractDomain(src) : L"unknown";
  CoTaskMemFree(src);

  std::wstring pwPath = GetPasswordPath();
  std::string domainA = WideToUTF8(domain.c_str());
  std::string encodedDomain = Base64Encode(domainA);

  HANDLE hFile = CreateFileW(pwPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hFile == INVALID_HANDLE_VALUE) {
    MessageBoxA(nullptr, "Geen opgeslagen wachtwoorden.", "Auto-fill", MB_OK);
    return;
  }

  DWORD sz = GetFileSize(hFile, nullptr);
  std::string data(static_cast<size_t>(sz), '\0');
  DWORD rd = 0;
  ReadFile(hFile, &data[0], sz, &rd, nullptr);
  CloseHandle(hFile);

  bool found = false;
  std::string userB64, passB64;
  size_t pos = 0;
  while (pos < data.size()) {
    size_t nl = data.find('\n', pos);
    if (nl == std::string::npos) break;
    std::string line = data.substr(pos, nl - pos);
    pos = nl + 1;
    if (line.empty()) continue;

    size_t s1 = line.find('\x01');
    if (s1 == std::string::npos) continue;
    size_t s2 = line.find('\x01', s1 + 1);
    if (s2 == std::string::npos) continue;

    std::string dom = line.substr(0, s1);
    if (dom == encodedDomain) {
      userB64 = line.substr(s1 + 1, s2 - s1 - 1);
      passB64 = line.substr(s2 + 1);
      found = true;
      break;
    }
  }

  if (!found) {
    MessageBoxA(nullptr, "Geen wachtwoord voor deze site.", "Auto-fill", MB_OK);
    return;
  }

  std::string userDecoded = Base64Decode(userB64);
  std::string passDecoded = Base64Decode(passB64);
  std::wstring wuser = UTF8ToWide(userDecoded);
  std::wstring wpass = UTF8ToWide(passDecoded);

  auto escape = [](std::wstring& s) {
    size_t p = 0;
    while ((p = s.find(L'\'', p)) != std::wstring::npos) {
      s.insert(p, 1, L'\\');
      p += 2;
    }
  };
  escape(wuser);
  escape(wpass);

  std::wstring script = L"(function(u,p){"
    L"var f=document.querySelector('form');"
    L"if(!f)return;"
    L"var inputs=f.querySelectorAll('input');"
    L"var fu=false,fp=false;"
    L"for(var i=0;i<inputs.length;i++){"
    L"var inp=inputs[i];"
    L"if(inp.type==='password'&&!fp){inp.value=p;fp=true;}"
    L"else if((inp.type==='email'||inp.type==='text')&&!fu){inp.value=u;fu=true;}"
    L"else if(inp.type==='email'||inp.type==='text'){inp.value=u;}"
    L"}"
    L"})('" + wuser + L"','" + wpass + L"')";

  webview_->ExecuteScript(script.c_str(), new PmAutofillHandler(this, wuser, wpass));
  MessageBoxA(nullptr, "Auto-fill uitgevoerd.", "Auto-fill", MB_OK);
}

void BrowserWindow::ViewPasswords() {
  std::wstring pwPath = GetPasswordPath();
  HANDLE hFile = CreateFileW(pwPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hFile == INVALID_HANDLE_VALUE) {
    MessageBoxA(nullptr, "Geen opgeslagen wachtwoorden.", "Wachtwoorden", MB_OK);
    return;
  }

  DWORD sz = GetFileSize(hFile, nullptr);
  std::string data(static_cast<size_t>(sz), '\0');
  DWORD rd = 0;
  ReadFile(hFile, &data[0], sz, &rd, nullptr);
  CloseHandle(hFile);

  std::string display;
  size_t pos = 0;
  while (pos < data.size()) {
    size_t nl = data.find('\n', pos);
    if (nl == std::string::npos) break;
    std::string line = data.substr(pos, nl - pos);
    pos = nl + 1;
    if (line.empty()) continue;

    size_t s1 = line.find('\x01');
    if (s1 == std::string::npos) continue;
    size_t s2 = line.find('\x01', s1 + 1);
    if (s2 == std::string::npos) continue;

    std::string domB64 = line.substr(0, s1);
    std::string userB64 = line.substr(s1 + 1, s2 - s1 - 1);
    std::string passB64 = line.substr(s2 + 1);

    std::string domain = Base64Decode(domB64);
    std::string user = Base64Decode(userB64);
    std::string pass = Base64Decode(passB64);

    display += domain + " | " + user + " | " + pass + "\n";
  }

  if (display.empty()) {
    MessageBoxA(nullptr, "Geen opgeslagen wachtwoorden.", "Wachtwoorden", MB_OK);
    return;
  }

  MessageBoxA(nullptr, display.c_str(), "Opgeslagen Wachtwoorden", MB_OK);
}

void BrowserWindow::ClearPasswords() {
  std::wstring pwPath = GetPasswordPath();
  if (DeleteFileW(pwPath.c_str())) {
    MessageBoxA(nullptr, "Alle wachtwoorden gewist.", "Wachtwoorden", MB_OK);
  } else {
    MessageBoxA(nullptr, "Geen wachtwoorden om te wissen.", "Wachtwoorden", MB_OK);
  }
}

// -------------------------------------------------------------------
// Chrome cookie import via SQLite parsing
// -------------------------------------------------------------------

// Read a SQLite varint, advances pointer
static uint64_t ReadVarint(const uint8_t*& p) {
  uint64_t v = 0;
  for (int i = 0; i < 9; i++) {
    v = (v << 7) | (*p & 0x7F);
    if (!(*p++ & 0x80)) break;
  }
  return v;
}

void BrowserWindow::ImportChromeCookies() {
  wchar_t chromePath[MAX_PATH];
  GetEnvironmentVariableW(L"LOCALAPPDATA", chromePath, MAX_PATH);
  wcscat_s(chromePath, L"\\Google\\Chrome\\User Data\\Default\\Cookies");

  // Copy to temp (Chrome might have the file locked)
  wchar_t tmpPath[MAX_PATH];
  GetTempPathW(MAX_PATH, tmpPath);
  wcscat_s(tmpPath, L"nara_chrome_cookies.tmp");
  if (!CopyFileW(chromePath, tmpPath, FALSE)) {
    MessageBoxA(nullptr, "Chrome cookies bestand niet gevonden.\nZorg dat Chrome bestaat en gesloten is.", "Chrome Import", MB_OK);
    return;
  }

  HANDLE hFile = CreateFileW(tmpPath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hFile == INVALID_HANDLE_VALUE) {
    MessageBoxA(nullptr, "Kan Chrome cookies niet lezen.", "Chrome Import", MB_OK);
    return;
  }

  DWORD fsize = GetFileSize(hFile, nullptr);
  if (fsize < 100) { CloseHandle(hFile); return; }

  std::vector<uint8_t> data(fsize);
  DWORD rd = 0;
  ReadFile(hFile, data.data(), fsize, &rd, nullptr);
  CloseHandle(hFile);
  DeleteFileW(tmpPath);

  // SQLite header
  if (memcmp(data.data(), "SQLite format 3\0", 16) != 0) {
    MessageBoxA(nullptr, "Geen geldig SQLite bestand.", "Chrome Import", MB_OK);
    return;
  }

  uint32_t pageSize = *(uint16_t*)(data.data() + 16); // big-endian
  pageSize = (pageSize << 8) | (pageSize >> 8);       // swap bytes
  if (pageSize == 1) pageSize = 65536;

  // Read sqlite_master to find the root page of the "cookies" table
  // Page 1 starts at offset 0 (first 100 bytes is header, rest is page data)
  uint32_t cookiesRootPage = 0;
  auto getPage = [&](uint32_t pg) -> const uint8_t* {
    return data.data() + (pg == 1 ? 0 : (pg - 1) * pageSize);
  };

  // Page 1: sqlite_master table (always leaf/interior table at page 1)
  const uint8_t* p1 = data.data() + (pageSize > 100 ? 100 : 0);
  // p1 now points to page 1 content (after the header)
  // The first byte is the page type
  // For simplicity, scan all leaf table pages for the cookies table
  // sqlite_master schema: type, name, tbl_name, rootpage, sql
  // We search for tbl_name = 'cookies'

  // Recursive search for the cookies table
  struct FindCookies {
    const std::vector<uint8_t>& data;
    uint32_t pageSize;
    uint32_t result = 0;

    FindCookies(const std::vector<uint8_t>& d, uint32_t ps) : data(d), pageSize(ps) {}

    void SearchPage(uint32_t pg) {
      if (pg == 0 || (pg - 1) * pageSize >= data.size()) return;
      const uint8_t* p = data.data() + (pg == 1 ? 0 : (pg - 1) * pageSize);
      uint8_t type = p[0];
      uint16_t cells = *(uint16_t*)(p + 3);
      cells = (cells >> 8) | (cells << 8);

      if (type == 0x0D) { // leaf table
        uint16_t* cellPtrs = (uint16_t*)(p + 8);
        for (int i = 0; i < cells; i++) {
          uint16_t off = cellPtrs[i];
          off = (off >> 8) | (off << 8);
          const uint8_t* cell = p + off;
          const uint8_t* cp = cell;
          ReadVarint(cp); // payload length
          ReadVarint(cp); // row id
          ReadVarint(cp); // header size
          // Read columns: type, name, tbl_name, rootpage, sql
          // Column 0 (type): serial type
          // Column 1 (name): 
          // Column 2 (tbl_name): 
          // Column 3 (rootpage): 
          // Read the 5 header serial types
          uint64_t ser[5];
          for (int j = 0; j < 5; j++) ser[j] = ReadVarint(cp);
          // Extract tbl_name (column 2, index 2)
          // Skip type (col 0)
          if (ser[0] >= 12) cp += (ser[0] - 12) / 2;
          // Skip name (col 1)
          if (ser[1] >= 12) cp += (ser[1] - 12) / 2;
          // Read tbl_name (col 2)
          if (ser[2] >= 12) {
            size_t len = (ser[2] - 12) / 2;
            if (len == 7 && memcmp(cp, "cookies", 7) == 0) {
              // Found! Now read rootpage (col 3)
              cp += len;
              if (ser[3] >= 12) cp += (ser[3] - 12) / 2;
              else if (ser[3] == 1) { result = *cp++; }
              // sqlite stores integers in big-endian
              if (ser[3] == 4) {
                result = (cp[0] << 24) | (cp[1] << 16) | (cp[2] << 8) | cp[3];
              } else if (ser[3] == 3) {
                result = (cp[0] << 16) | (cp[1] << 8) | cp[2];
              } else if (ser[3] == 2) {
                result = (cp[0] << 8) | cp[1];
              } else if (ser[3] == 1) {
                result = cp[0];
              }
            }
          }
          if (result) return;
        }
      } else if (type == 0x05) { // interior table
        uint16_t* cellPtrs = (uint16_t*)(p + 8);
        for (int i = 0; i < cells; i++) {
          uint16_t off = cellPtrs[i];
          off = (off >> 8) | (off << 8);
          const uint8_t* cell = p + off;
          uint32_t childPage = (cell[0] << 24) | (cell[1] << 16) | (cell[2] << 8) | cell[3];
          const uint8_t* cp = cell + 4;
          ReadVarint(cp); // payload len
          ReadVarint(cp); // row id
          ReadVarint(cp); // header size
          uint64_t ser[5];
          for (int j = 0; j < 5; j++) ser[j] = ReadVarint(cp);
          if (ser[0] >= 12) cp += (ser[0] - 12) / 2;
          if (ser[1] >= 12) cp += (ser[1] - 12) / 2;
          if (ser[2] >= 12) {
            size_t len = (ser[2] - 12) / 2;
            if (len == 7 && memcmp(cp, "cookies", 7) == 0) {
              if (ser[3] >= 12) cp += (ser[3] - 12) / 2;
              if (ser[3] == 4) {
                result = (cp[0] << 24) | (cp[1] << 16) | (cp[2] << 8) | cp[3];
              } else if (ser[3] == 3) {
                result = (cp[0] << 16) | (cp[1] << 8) | cp[2];
              } else if (ser[3] == 2) {
                result = (cp[0] << 8) | cp[1];
              } else if (ser[3] == 1) {
                result = cp[0];
              }
            }
          }
          if (result) return;
          SearchPage(childPage);
          if (result) return;
        }
      }
    }
  };

  FindCookies finder(data, pageSize);
  finder.SearchPage(1);
  uint32_t rootPage = finder.result;

  if (!rootPage) {
    MessageBoxA(nullptr, "Geen cookies tabel gevonden in Chrome.", "Chrome Import", MB_OK);
    return;
  }

  // Now iterate the cookies table leaf pages
  if (!webview_) return;
  ICoreWebView2_2* wv2 = nullptr;
  HRESULT hr = webview_->QueryInterface(IID_ICoreWebView2_2, (void**)&wv2);
  if (FAILED(hr) || !wv2) {
    MessageBoxA(nullptr, "Kan CookieManager niet openen.", "Chrome Import", MB_OK);
    return;
  }
  ICoreWebView2CookieManager* mgr = nullptr;
  wv2->get_CookieManager(&mgr);
  wv2->Release();
  if (!mgr) return;

  UINT32 imported = 0;
  // Process leaf pages
  struct CookieWalker {
    const std::vector<uint8_t>& data;
    uint32_t pageSize;
    ICoreWebView2CookieManager* mgr;
    UINT32& imported;

    void Walk(uint32_t pg) {
      if (pg == 0 || (pg - 1) * pageSize >= data.size()) return;
      const uint8_t* p = data.data() + (pg - 1) * pageSize;
      uint8_t type = p[0];
      uint16_t cells = *(uint16_t*)(p + 3);
      cells = (cells >> 8) | (cells << 8);
      uint16_t* cellPtrs = (uint16_t*)(p + 8);

      if (type == 0x0D) {
        for (int i = 0; i < cells; i++) {
          uint16_t off = cellPtrs[i];
          off = (off >> 8) | (off << 8);
          const uint8_t* cell = p + off;
          const uint8_t* cp = cell;
          ReadVarint(cp); // payload length
          ReadVarint(cp); // row id
          ReadVarint(cp); // header size = N_cols + 1 varints
          // We know the cookies table has 18 columns (Chrome)
          // Read serial types for columns 1-17 (skip col 0 creation_utc)
          uint64_t ser[18];
          int nSer = 0;
          while (nSer < 18 && cp < p + pageSize) {
            if (nSer == 0) { ser[nSer++] = ReadVarint(cp); continue; } // header size varint
            ser[nSer++] = ReadVarint(cp);
          }
          // Now skip column 0 (creation_utc) data
          // Process columns 1-17 in order
          std::string host_key, name, value, path;
          int64_t expires_utc = 0;
          int is_secure = 0, is_httponly = 0, has_expires = 1, samesite = -1;
          int colIdx = 0;

          auto readText = [&](uint64_t st) -> std::string {
            if (st < 12) return {};
            size_t len = (st - 12) / 2;
            std::string s(reinterpret_cast<const char*>(cp), len);
            cp += len;
            return s;
          };
          auto readInt = [&](uint64_t st) -> int64_t {
            int64_t v = 0;
            if (st == 0) return 0;
            if (st == 1) { v = (int8_t)*cp; cp += 1; }
            else if (st == 2) { v = (int16_t)((cp[0] << 8) | cp[1]); cp += 2; }
            else if (st == 3) { v = (int32_t)((cp[0] << 16) | (cp[1] << 8) | cp[2]); if (v & 0x800000) v |= ~0xffffff; cp += 3; }
            else if (st == 4) { v = (int32_t)((cp[0] << 24) | (cp[1] << 16) | (cp[2] << 8) | cp[3]); cp += 4; }
            else if (st == 5) { cp += 6; }
            else if (st == 6) {
              uint64_t uv = ((uint64_t)cp[0] << 56) | ((uint64_t)cp[1] << 48) |
                            ((uint64_t)cp[2] << 40) | ((uint64_t)cp[3] << 32) |
                            ((uint64_t)cp[4] << 24) | ((uint64_t)cp[5] << 16) |
                            ((uint64_t)cp[6] << 8)  | (uint64_t)cp[7];
              v = (int64_t)uv; cp += 8;
            }
            else if (st == 7) { cp += 8; }
            else if (st == 8) { v = 0; }
            else if (st == 9) { v = 1; }
            return v;
          };

          // Col 0: creation_utc (INTEGER) - skip
          if (ser[1+0] >= 12) cp += (ser[1+0] - 12) / 2;
          else readInt(ser[1+0]);

          // Col 1: host_key (TEXT)
          if (colIdx < 18 && ser[1+1] >= 12) { host_key = readText(ser[1+1]); colIdx++; } else colIdx++;
          // Col 2: top_frame_site_key - skip
          if (colIdx < 18 && ser[1+2] >= 12) { cp += (ser[1+2] - 12) / 2; colIdx++; } else colIdx++;
          // Col 3: name (TEXT)
          if (colIdx < 18 && ser[1+3] >= 12) { name = readText(ser[1+3]); colIdx++; } else colIdx++;
          // Col 4: value (TEXT)
          if (colIdx < 18 && ser[1+4] >= 12) { value = readText(ser[1+4]); colIdx++; } else colIdx++;
          // Col 5: path (TEXT)
          if (colIdx < 18 && ser[1+5] >= 12) { path = readText(ser[1+5]); colIdx++; } else colIdx++;
          // Col 6: expires_utc (INTEGER)
          if (colIdx < 18) { expires_utc = readInt(ser[1+6]); colIdx++; } else colIdx++;
          // Col 7: is_secure (INTEGER)
          if (colIdx < 18) { is_secure = (int)readInt(ser[1+7]); colIdx++; } else colIdx++;
          // Col 8: is_httponly (INTEGER)
          if (colIdx < 18) { is_httponly = (int)readInt(ser[1+8]); colIdx++; } else colIdx++;
          // Skip remaining columns...

          if (host_key.empty() || name.empty()) continue;

          // Prepend '.' for WebView2 domain format
          std::string domain = host_key;

          ICoreWebView2Cookie* c = nullptr;
          std::wstring wdomain = UTF8ToWide(domain);
          std::wstring wname = UTF8ToWide(name);
          std::wstring wvalue = UTF8ToWide(value);
          std::wstring wpath = UTF8ToWide(path.empty() ? "/" : path);

          HRESULT hr2 = mgr->CreateCookie(wname.c_str(), wvalue.c_str(),
                                           wdomain.c_str(), wpath.c_str(), &c);
          if (FAILED(hr2) || !c) continue;

          c->put_IsSecure(is_secure ? TRUE : FALSE);
          c->put_IsHttpOnly(is_httponly ? TRUE : FALSE);
          if (samesite >= 0)
            c->put_SameSite(static_cast<COREWEBVIEW2_COOKIE_SAME_SITE_KIND>(samesite));
          if (has_expires && expires_utc > 0) {
            // Chrome time: microseconds since 1601-01-01
            // WebView2: seconds since 1601-01-01
            c->put_Expires((double)expires_utc / 1000000.0);
          }

          mgr->AddOrUpdateCookie(c);
          c->Release();
          imported++;
        }
      } else if (type == 0x05) {
        for (int i = 0; i < cells; i++) {
          uint16_t off = cellPtrs[i];
          off = (off >> 8) | (off << 8);
          const uint8_t* cell = p + off;
          uint32_t childPage = (cell[0] << 24) | (cell[1] << 16) | (cell[2] << 8) | cell[3];
          Walk(childPage);
        }
      }
    }
  };

  CookieWalker walker{data, pageSize, mgr, imported};
  walker.Walk(rootPage);
  mgr->Release();

  char msg[64];
  snprintf(msg, sizeof(msg), "%u cookies geimporteerd uit Chrome.", imported);
  MessageBoxA(nullptr, msg, "Chrome Import", MB_OK);
}


