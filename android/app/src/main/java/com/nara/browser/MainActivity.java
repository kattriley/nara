package com.nara.browser;

import android.annotation.SuppressLint;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.webkit.CookieManager;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.EditText;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.webkit.WebSettings;
import androidx.webkit.WebViewFeature;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.Base64;
import java.util.Locale;

public class MainActivity extends AppCompatActivity {

    private enum Lang { English, Nederlands, Polski }
    private Lang currentLang = Lang.English;

    private LinearLayout tabBar;
    private LinearLayout toolbar;
    private EditText urlBar;
    private ViewGroup webViewContainer;
    private Button goButton, backButton, forwardButton, reloadButton, settingsButton;

    private static class TabInfo {
        WebView webView;
        String title;
        String url;
        TabInfo(WebView w, String u) { webView = w; url = u; title = ""; }
    }

    private final ArrayList<TabInfo> tabs = new ArrayList<>();
    private int activeTab = -1;
    private boolean isDark = false;

    private SharedPreferences prefs;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        prefs = getSharedPreferences("nara", MODE_PRIVATE);
        isDark = prefs.getBoolean("dark", false);

        tabBar = findViewById(R.id.tabBar);
        toolbar = findViewById(R.id.toolbar);
        urlBar = findViewById(R.id.urlBar);
        webViewContainer = findViewById(R.id.webViewContainer);
        goButton = findViewById(R.id.goButton);
        backButton = findViewById(R.id.backButton);
        forwardButton = findViewById(R.id.forwardButton);
        reloadButton = findViewById(R.id.reloadButton);
        settingsButton = findViewById(R.id.settingsButton);

        urlBar.setOnEditorActionListener((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_GO || actionId == EditorInfo.IME_ACTION_DONE) {
                navigateTo(urlBar.getText().toString());
                return true;
            }
            return false;
        });

        goButton.setOnClickListener(v -> navigateTo(urlBar.getText().toString()));
        backButton.setOnClickListener(v -> { if (activeTab >= 0 && tabs.get(activeTab).webView.canGoBack()) tabs.get(activeTab).webView.goBack(); });
        forwardButton.setOnClickListener(v -> { if (activeTab >= 0 && tabs.get(activeTab).webView.canGoForward()) tabs.get(activeTab).webView.goForward(); });
        reloadButton.setOnClickListener(v -> { if (activeTab >= 0) tabs.get(activeTab).webView.reload(); });
        settingsButton.setOnClickListener(v -> showSettingsMenu());

        findViewById(R.id.newTabButton).setOnClickListener(v -> newTab("https://www.google.com"));

        newTab("https://www.google.com");
        applyTheme();
    }

    private void navigateTo(String input) {
        String url = input.trim();
        if (url.isEmpty()) return;
        if (!url.startsWith("http://") && !url.startsWith("https://")) {
            if (url.contains(".") && !url.contains(" ")) url = "https://" + url;
            else url = "https://www.google.com/search?q=" + url;
        }
        if (activeTab >= 0) {
            tabs.get(activeTab).webView.loadUrl(url);
            urlBar.setText(url);
        }
    }

    @SuppressLint("SetJavaScriptEnabled")
    private void newTab(String url) {
        WebView wv = new WebView(this);
        wv.getSettings().setJavaScriptEnabled(true);
        wv.getSettings().setAllowFileAccess(true);
        wv.getSettings().setDomStorageEnabled(true);
        wv.getSettings().setMixedContentMode(android.webkit.WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);
        if (WebViewFeature.isFeatureSupported(WebViewFeature.FORCE_DARK)) {
            androidx.webkit.WebSettingsCompat.setForceDark(wv.getSettings(),
                isDark ? androidx.webkit.WebSettingsCompat.FORCE_DARK_ON : androidx.webkit.WebSettingsCompat.FORCE_DARK_OFF);
        }

        wv.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageStarted(WebView view, String url, Bitmap favicon) {
                urlBar.setText(url);
                int idx = tabs.indexOf(new TabInfo(wv, ""));
                if (idx >= 0) tabs.get(idx).url = url;
            }
            @Override
            public void onPageFinished(WebView view, String url) {
                int idx = tabs.indexOf(new TabInfo(wv, ""));
                if (idx >= 0) {
                    tabs.get(idx).title = view.getTitle();
                    tabs.get(idx).url = url;
                }
                rebuildTabBar();
            }
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                return false;
            }
        });

        wv.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onReceivedTitle(WebView view, String title) {
                int idx = tabs.indexOf(new TabInfo(wv, ""));
                if (idx >= 0) tabs.get(idx).title = title;
                rebuildTabBar();
            }
        });

        // Hide the current tab's webview
        if (activeTab >= 0 && activeTab < tabs.size()) {
            webViewContainer.removeView(tabs.get(activeTab).webView);
        }

        TabInfo tab = new TabInfo(wv, url);
        tabs.add(tab);
        activeTab = tabs.size() - 1;

        webViewContainer.addView(wv, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        wv.loadUrl(url);
        urlBar.setText(url);
        rebuildTabBar();
    }

    private void closeTab(int idx) {
        if (tabs.size() <= 1) return;
        if (idx < 0 || idx >= tabs.size()) return;

        webViewContainer.removeView(tabs.get(idx).webView);
        tabs.get(idx).webView.destroy();
        tabs.remove(idx);

        if (activeTab >= tabs.size()) activeTab = tabs.size() - 1;
        if (idx < activeTab) activeTab--;
        if (activeTab >= 0 && activeTab < tabs.size()) {
            webViewContainer.addView(tabs.get(activeTab).webView, new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT));
            urlBar.setText(tabs.get(activeTab).url);
        }
        rebuildTabBar();
    }

    private void switchTab(int idx) {
        if (idx < 0 || idx >= tabs.size() || idx == activeTab) return;
        if (activeTab >= 0 && activeTab < tabs.size()) {
            webViewContainer.removeView(tabs.get(activeTab).webView);
        }
        activeTab = idx;
        webViewContainer.addView(tabs.get(idx).webView, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        urlBar.setText(tabs.get(idx).url);
        rebuildTabBar();
    }

    private void rebuildTabBar() {
        tabBar.removeAllViews();

        Button newBtn = new Button(this);
        newBtn.setText("+");
        newBtn.setTextSize(14);
        newBtn.setPadding(8, 0, 8, 0);
        newBtn.setOnClickListener(v -> newTab("https://www.google.com"));
        tabBar.addView(newBtn);

        for (int i = 0; i < tabs.size(); i++) {
            String label = tabs.get(i).title;
            if (label == null || label.isEmpty()) label = t("Tab ") + (i + 1);
            if (label.length() > 12) label = label.substring(0, 10) + "..";

            final int fi = i;

            Button tabBtn = new Button(this);
            tabBtn.setText(label);
            tabBtn.setTextSize(12);
            tabBtn.setPadding(10, 0, 10, 0);
            tabBtn.setSingleLine(true);
            if (i == activeTab) tabBtn.setActivated(true);
            tabBtn.setOnClickListener(v -> switchTab(fi));
            tabBar.addView(tabBtn);

            Button closeBtn = new Button(this);
            closeBtn.setText("×");
            closeBtn.setTextSize(12);
            closeBtn.setPadding(4, 0, 4, 0);
            closeBtn.setOnClickListener(v -> closeTab(fi));
            tabBar.addView(closeBtn);
        }
    }

    private String t(int id) {
        String[][] strings = getStrings();
        int lang = currentLang.ordinal();
        if (id < 0 || id >= strings[0].length) return "";
        return strings[lang][id];
    }

    private static final int S_GO = 1, S_SEARCH = 2, S_SETTINGS = 3, S_GOOGLE = 4, S_PINTEREST = 5,
            S_ROBLOX = 6, S_DISCORD = 7, S_SPOTIFY = 8, S_DOWNLOADS = 9, S_DARK = 10,
            S_IMPORT = 11, S_EXPORT = 12, S_NEW_TAB = 13, S_LIGHT_MODE = 14, S_DARK_MODE = 15,
            S_LANGUAGE = 16, S_ENGLISH = 17, S_NEDERLANDS = 18, S_POLSKI = 19, S_CREDITS = 20,
            S_NARA_BROWSER = 21, S_MADE_BY = 22, S_POWERED_BY = 23, S_LANG_SET_EN = 24,
            S_LANG_SET_NL = 25, S_LANG_SET_PL = 26, S_LANG_TITLE = 27,
            S_COOKIE_IMPORT_TITLE = 28, S_COOKIE_EXPORT_OK = 29, S_COOKIE_IMPORT_OK = 30,
            S_COOKIE_NO_FILE = 31, S_PW_SAVED = 32, S_PW_NO_FORM = 33, S_PW_FILL_FORM = 34,
            S_PW_VIEW_TITLE = 35, S_PW_NO_PASSWORDS = 36, S_PW_CLEAR_OK = 37, S_PW_CLEAR_NONE = 38,
            S_PW_AUTOFILL_OK = 39, S_PW_AUTOFILL_NONE = 40, S_BM_ADDED = 41, S_BM_NONE = 42,
            S_BM_TITLE = 43, S_DL_COMING = 44, S_CREDITS_TEXT = 45, S_CREDITS_TITLE = 46,
            S_TAB_PREFIX = 47, S_NIEUWE_TAB = 48, S_ERROR = 49;

    private String[][] getStrings() {
        return new String[][]{
                // English
                {"", "Go", "🔍", "⚙", "🔍 Google", "📌 Pinterest", "⭐ Roblox", "🎮 Discord",
                 "🎵 Spotify", "📩 Downloads", "🌙 Dark", "📥 Import", "📤 Export", "+",
                 "Light Mode", "Dark Mode", "Language", "English", "Nederlands", "Polski",
                 "Credits", "Nara Browser", "Made by yutaa", "Powered by WebView2",
                 "Language set to English.", "Language set to Nederlands.", "Language set to Polski.",
                 "Language", "Import", "Cookies exported", "cookies imported.", "No cookies.txt found.",
                 "Password saved!", "No login form found.", "Fill in username and password first.",
                 "Saved Passwords", "No saved passwords.", "All passwords cleared.",
                 "No passwords to clear.", "Auto-fill done.", "No password saved for this site.",
                 "Bookmark added!", "No bookmarks.", "Bookmarks", "Downloads coming soon!",
                 "Nara Browser\n\nMade by yutaa", "Credits", "Tab ", "New tab", "Error"},
                // Nederlands
                {"", "Go", "🔍", "⚙", "🔍 Google", "📌 Pinterest", "⭐ Roblox", "🎮 Discord",
                 "🎵 Spotify", "📩 Downloads", "🌙 Donker", "📥 Importeren", "📤 Exporteren", "+",
                 "Lichte modus", "Donkere modus", "Taal", "English", "Nederlands", "Polski",
                 "Credits", "Nara Browser", "Gemaakt door yutaa", "Mogelijk gemaakt door WebView2",
                 "Taal ingesteld op Engels.", "Taal ingesteld op Nederlands.", "Taal ingesteld op Pools.",
                 "Taal", "Importeren", "Cookies geëxporteerd", "cookies geïmporteerd.", "Geen cookies.txt gevonden.",
                 "Wachtwoord opgeslagen!", "Geen inlogformulier gevonden.", "Vul eerst gebruikersnaam en wachtwoord in.",
                 "Opgeslagen Wachtwoorden", "Geen opgeslagen wachtwoorden.", "Alle wachtwoorden gewist.",
                 "Geen wachtwoorden om te wissen.", "Auto-fill uitgevoerd.", "Geen wachtwoord voor deze site.",
                 "Bookmark toegevoegd!", "Geen bookmarks.", "Bookmarks", "Downloads komen binnenkort!",
                 "Nara Browser\n\nGemaakt door yutaa", "Credits", "Tab ", "Nieuwe tab", "Fout"},
                // Polski
                {"", "Idź", "🔍", "⚙", "🔍 Google", "📌 Pinterest", "⭐ Roblox", "🎮 Discord",
                 "🎵 Spotify", "📩 Pobieranie", "🌙 Ciemny", "📥 Importuj", "📤 Eksportuj", "+",
                 "Jasny tryb", "Ciemny tryb", "Język", "English", "Nederlands", "Polski",
                 "Twórcy", "Nara Browser", "Stworzone przez yutaa", "Działa na WebView2",
                 "Język zmieniony na angielski.", "Język zmieniony na niderlandzki.", "Język zmieniony na polski.",
                 "Język", "Importuj", "Cookies wyeksportowane", "cookies zaimportowane.", "Nie znaleziono cookies.txt.",
                 "Hasło zapisane!", "Nie znaleziono formularza logowania.", "Wpisz najpierw nazwę użytkownika i hasło.",
                 "Zapisane Hasła", "Brak zapisanych haseł.", "Wszystkie hasła usunięte.",
                 "Brak haseł do usunięcia.", "Auto-wypełnienie wykonane.", "Brak hasła dla tej strony.",
                 "Zakładka dodana!", "Brak zakładek.", "Zakładki", "Pobieranie wkrótce!",
                 "Nara Browser\n\nStworzone przez yutaa", "Twórcy", "Karta ", "Nowa karta", "Błąd"},
        };
    }

    private void showSettingsMenu() {
        String[] items = {
                isDark ? t(S_LIGHT_MODE) : t(S_DARK_MODE),
                t(S_LANGUAGE) + " → " + t(S_ENGLISH) + " / " + t(S_NEDERLANDS) + " / " + t(S_POLSKI),
                t(S_CREDITS)
        };
        new AlertDialog.Builder(this)
                .setTitle(t(S_SETTINGS))
                .setItems(items, (dialog, which) -> {
                    if (which == 0) {
                        isDark = !isDark;
                        prefs.edit().putBoolean("dark", isDark).apply();
                        applyTheme();
                    } else if (which == 1) {
                        showLanguageMenu();
                    } else if (which == 2) {
                        showToast(t(S_CREDITS_TEXT));
                    }
                })
                .show();
    }

    private void showLanguageMenu() {
        String[] langs = {t(S_ENGLISH), t(S_NEDERLANDS), t(S_POLSKI)};
        new AlertDialog.Builder(this)
                .setTitle(t(S_LANGUAGE))
                .setItems(langs, (dialog, which) -> {
                    if (which == 0) currentLang = Lang.English;
                    else if (which == 1) currentLang = Lang.Nederlands;
                    else currentLang = Lang.Polski;
                    rebuildTabBar();
                    showToast(currentLang == Lang.English ? t(S_LANG_SET_EN) :
                            currentLang == Lang.Nederlands ? t(S_LANG_SET_NL) : t(S_LANG_SET_PL));
                })
                .show();
    }

    private void applyTheme() {
        if (isDark) {
            findViewById(android.R.id.content).getRootView().setBackgroundColor(0xFF1e1e1e);
        } else {
            findViewById(android.R.id.content).getRootView().setBackgroundColor(0xFFFFFFFF);
        }
        for (TabInfo tab : tabs) {
            if (tab.webView != null && WebViewFeature.isFeatureSupported(WebViewFeature.FORCE_DARK)) {
                androidx.webkit.WebSettingsCompat.setForceDark(tab.webView.getSettings(),
                    isDark ? androidx.webkit.WebSettingsCompat.FORCE_DARK_ON : androidx.webkit.WebSettingsCompat.FORCE_DARK_OFF);
            }
        }
    }

    // Quick link handlers (called from layout XML)
    public void launchGoogle(View v) { navigateTo("https://www.google.com"); }
    public void launchPinterest(View v) { navigateTo("https://www.pinterest.com"); }
    public void launchRoblox(View v) { navigateTo("https://www.roblox.com"); }
    public void launchDiscord(View v) { navigateTo("https://discord.com"); }
    public void launchSpotify(View v) { navigateTo("https://open.spotify.com"); }

    private void showToast(String msg) {
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onBackPressed() {
        if (activeTab >= 0 && tabs.get(activeTab).webView.canGoBack()) {
            tabs.get(activeTab).webView.goBack();
        } else {
            super.onBackPressed();
        }
    }
}
