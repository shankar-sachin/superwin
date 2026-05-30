# SuperWin

A Windows desktop multi-tool (C++ / C++/WinRT, WinUI 3) that lives in the system
tray and bundles four utilities:

- **Diagnostics + Mini-monitor** — CPU/GPU/RAM/disk info and a live, always-on-top
  mini-monitor window.
- **Clipboard++** — clipboard history with a global hotkey picker (`Win+Shift+V`).
- **Notepad Super** — a "Word Lite" rich-text editor (WinUI `RichEditBox`).
- **Volume Customizer** — per-app and master volume in exact percent and decibels.

## Architecture

A single `SuperWin.exe` process: a tray icon + a tabbed dashboard, with Clipboard++
and the mini-monitor running as background features. The UI is built **code-first in
C++/WinRT** (no XAML markup files), which keeps the CMake build free of the XAML
compiler. See `plan` notes and source comments for the per-module Windows-API design.

```
src/
  main.cpp                  WinMain: bootstrap, single-instance, tray, hotkeys
  core/                     Settings, SingleInstance, Hotkeys, TrayIcon, Autostart
  app/                      App, MainWindow (NavigationView shell), Theme
  modules/
    diagnostics/            HardwareProbe (WMI/DXGI/PDH), DiagPage, MiniMonitor
    clipboard/              ClipWatcher, ClipStore, ClipPicker, ClipPage
    notepad/                EditorPage (RichEditBox), DocumentIO
    volume/                 AudioSessions (WASAPI), VolumeMath, VolumePage
tests/                      Catch2 unit tests (VolumeMath, ClipStore)
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

Current version: **1.0.0** (`src/Version.h` is the single source of truth; the exe
carries a matching `VERSIONINFO` resource).

SuperWin auto-updates via **WinSparkle**. The installed app polls an *appcast* feed
(every 24h, or on demand via the tray's **Check for updates…**); when a newer
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

**v1.0.0 foundation complete and verified:** CMake build, passing unit tests
(`superwin_core`), a working tray host, a styled WinUI 3 dashboard shell
(self-contained, code-first), WinSparkle auto-updates, and a per-user Inno Setup
wizard. The four module UIs are implemented next, in the order
Volume → Clipboard → Diagnostics → Notepad. See the plan for details.
