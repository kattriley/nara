# Nara

Een minimalistische web browser gebouwd op Microsoft Edge WebView2, geschreven in C++ met de Win32 API.

Gemaakt door **yutaa** — github.com/kattriley

## Downloads

Download de nieuwste release van [releases](https://github.com/kattriley/nara/releases).  
Zet `nara.exe` en `WebView2Loader.dll` in dezelfde map en start `nara.exe`.

## Features

- **WebView2 engine** — zelfde rendering engine als Microsoft Edge
- **Adresbalk** — typ een URL en druk op Enter of klik Go (https:// wordt automatisch toegevoegd)
- **Meerdere tabs** — klik \[+\] voor een nieuwe tab, klik tabs om te wisselen
- **Terug / Vooruit / Herladen** — navigatieknoppen in de toolbar
- **Zoeken op pagina** — 🔍 opent een JS prompt en markeert tekst op de pagina
- **Snelkoppelingen** — 🔍 Google, 📌 Pinterest, ⭐ Roblox, 🎮 Discord, 🎵 Spotify
- **Downloads** — 📩 (placeholder)
- **Dark mode** — 🌙 past de hele WebView aan (nachtmodus)
- **Site dark mode** — 🌙 in toolbar, aparte toggle per site (invert/hue-rotate)
- **Cookie manager** — 📥 import / 📤 export naar `%APPDATA%\Nara\cookies.txt`
- **Wachtwoordenmanager** — sla inloggegevens op (Base64 opgeslagen in `%APPDATA%\Nara\passwords.txt`), vul automatisch in, bekijk en wis
- **Bookmarks** — voeg toe en bekijk via `%APPDATA%\Nara\bookmarks.txt`
- **Dark/Light thema** — toggle via het Settings menu (⚙)
- **Taal** — Engels / Nederlands via Settings menu
- **Credits** — ⚙ → Credits
- **Ctrl+R** — herlaad huidige pagina
- **Inter font** — wordt automatisch gedownload van GitHub indien niet geïnstalleerd
- **Geen console** — `-mwindows` flag, schone Windows applicatie

## Builden

### Vereisten

- LLVM MinGW toolchain (getest met [llvm-mingw](https://github.com/mstorsjo/llvm-mingw))
- CMake 3.20+
- WebView2 SDK (wordt automatisch gedownload via CMake)

### Builden met CMake

```bash
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-clang++ -DCMAKE_RC_COMPILER=windres
cmake --build build
```

### Builden zonder CMake

```bash
windres -O coff src/resources.rc build/resources.res
x86_64-w64-mingw32-clang++ -std=c++17 -O2 -s -static -fuse-ld=lld -mwindows \
    -I src -I build/webview2/build/native/include \
    -o build/nara.exe \
    src/main.cpp src/browser_window.cpp build/resources.res \
    -Lbuild -Lbuild/webview2/build/native/x64 \
    -l:WebView2Loader.dll.lib -lgdi32 -lole32 -loleaut32 -luuid -lcomctl32 \
    -static-libgcc -static-libstdc++
```

## Bestandsstructuur

```
src/
├── main.cpp              — WinMain, WndProc, toolbar creatie, tab bar
├── main.h                — button IDs, constanten
├── browser_window.cpp    — BrowserWindow: tabs, adresbalk, navigatie, cookies, wachtwoorden, bookmarks
├── browser_window.h      — BrowserWindow class, TabInfo struct
└── resources.rc          — (optioneel) icoon resources
```

## Licentie

MIT
