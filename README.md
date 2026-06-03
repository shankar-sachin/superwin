# SuperWin

A Windows desktop multi-tool (C++ / C++/WinRT, WinUI 3) with a blended, custom
title bar and a built-in **Settings** page. It bundles:

- **Volume Customizer** — per-app and master volume in exact percent and decibels.
- **Clipboard** — process-wide clipboard history with a global quick-picker
  (`Win+Shift+V`): search, pinned items, keyboard navigation, and auto-paste back
  into the app you were using.
- **Diagnostics + Mini-monitor** — CPU/GPU/RAM/disk info and a live, always-on-top
  mini-monitor window.
- **Notepad Super** — a "Word Lite" rich-text editor (WinUI `RichEditBox`).
- **Color Picker** — pick any on-screen color; copy hex/RGB.
- **Keep Awake** — stop sleep/screen-off, optionally for a fixed duration.
- **Hash & Checksum** — MD5 / SHA-1 / SHA-256 of text or files (CNG/BCrypt).
- **Network Info** — adapters, IPv4/IPv6, MAC, and a quick ping.
- **Unit Converter** — length/mass/temperature/data-size units and number bases.
- **Password Generator** — configurable length and character classes with a strength readout.
- **Text** — case conversion, trimming, counting, and other quick text transforms.

## Architecture

A single `SuperWin.exe` process hosting a code-first WinUI 3 dashboard
(`NavigationView` shell). An `AppHost` owns a hidden Win32 message window on the UI
thread that registers the global `Win+Shift+V` hotkey and listens for OS clipboard
changes (`AddClipboardFormatListener`), feeding a **process-wide** `SharedClipStore`
for the whole session. The UI is built **code-first in C++/WinRT** (no XAML markup
files), which keeps the CMake build free of the XAML compiler.

```
src/
  main.cpp                  WinMain / WinUI App: bootstrap, single-instance, AppHost
  app/
    Shell                   NavigationView shell + blended (extended) title bar
    AppHost                 hidden Win32 host: global hotkey + clipboard listener
    HomePage, SettingsPage  dashboard home tiles + Settings (theme, clipboard, etc.)
  core/                     Settings, SingleInstance, Hotkeys, TrayIcon, Autostart
  modules/
    volume/                 AudioSessions (WASAPI), VolumeMath, VolumePage
    clipboard/              ClipStore, ClipText, ClipPage, ClipPicker (quick picker)
    diagnostics/            HardwareProbe (WMI/DXGI/PDH), DiagPage, MiniMonitor
    notepad/                EditorPage (RichEditBox)
    colorpicker/            ColorPickerPage
    keepawake/              KeepAwakePage (SetThreadExecutionState)
    hash/                   HashLogic (CNG/BCrypt), HashPage
    netinfo/                NetInfoLogic (IP Helper/Winsock), NetInfoPage
    convert/                ConvertLogic, ConvertPage
    password/               PasswordLogic, PasswordPage
    text/                   TextLogic, TextPage
tests/                      Catch2 unit tests (VolumeMath, ClipStore, HashLogic,
                            ConvertLogic, PasswordLogic, TextLogic)
installer/SuperWin.iss      Inno Setup wizard script
```

## Prerequisites

- **Visual Studio 2026** with the **Desktop development with C++** workload
  (MSVC toolset + Windows 11 SDK + CMake + Ninja).
- **Inno Setup 6** (for building the installer) — `winget install JRSoftware.InnoSetup`.

Third-party libraries are pulled automatically via **NuGet `PackageReference`** (wired
through CMake `VS_PACKAGE_REFERENCES`): Windows App SDK, C++/WinRT, WIL, nlohmann.json,
and Catch2. No vcpkg or manual restore step is required.

## Build

From a *Developer Command Prompt for VS 2026* (or any shell with CMake on PATH):

```powershell
cmake --preset msvc                       # configure (Visual Studio generator)
cmake --build --preset msvc-release       # build Release
```

The executable is produced under `build\Release\SuperWin.exe`.

The WinUI 3 dashboard is gated behind a CMake option while the Windows App SDK
integration is brought up in stages; enable it with:

```powershell
cmake --preset msvc -DSUPERWIN_ENABLE_WINUI=ON
```

### Run the tests

```powershell
ctest --preset msvc-release
```

### Build the installer

The app is built **self-contained** (the Windows App Runtime ships inside the app
folder), so no runtime redist staging is required. Build the wizard with Inno Setup:

```powershell
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" installer\SuperWin.iss
```

Output: `installer\Output\SuperWin-Setup.exe` — a per-user wizard (no UAC) with a
clean uninstaller.

## Versioning & auto-updates

Current version: **2.1.3** (`src/Version.h` is the single source of truth; the exe
carries a matching `VERSIONINFO` resource).

SuperWin auto-updates via **WinSparkle**. The installed app polls an *appcast* feed
(every 24h, or on demand via **Settings → Check for updates**); when a newer
`sparkle:version` is published it prompts the user, downloads the new
`SuperWin-Setup.exe`, and runs it to upgrade in place.

**To ship an update:**
1. Bump the version in `src/Version.h` **and** `installer/SuperWin.iss`.
2. Rebuild and produce a new `installer/Output/SuperWin-Setup.exe`.
3. Upload that setup.exe to your host (GitHub Releases, CDN, …).
4. Add a new `<item>` to `installer/appcast.xml` (new `sparkle:version` + download
   URL) and publish it at the appcast URL the app points to.

The appcast URL defaults to `kDefaultAppcastUrl` in `src/core/Updater.cpp` and is
overridable at runtime via the `update.appcastUrl` setting. See
`installer/appcast.xml` for the feed format. (For production, sign updates with an
EdDSA key via `win_sparkle_set_eddsa_*` and add `sparkle:edSignature` to the feed.)

## Status

**v2.1.3:** Eleven tools (Volume, Clipboard, Diagnostics, Notepad, Color Picker,
Keep Awake, Hash & Checksum, Network Info, Unit Converter, Password Generator, Text),
a blended custom title bar, a Settings page (theme, startup, clipboard options), and a
working `Win+Shift+V` quick-picker backed by process-wide clipboard capture.
Self-contained, code-first WinUI 3, WinSparkle auto-updates, per-user Inno Setup
wizard, and passing Catch2 unit tests. This patch lets you set the picker hotkey by
pressing the keys, adds live GPU/VRAM and disk activity/throughput to Diagnostics, and
ships a redesigned CPU/RAM/GPU/Disk mini-monitor that keeps running off-page.
