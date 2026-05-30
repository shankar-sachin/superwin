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

Stage the Windows App SDK runtime redist into `installer\redist\` as
`WindowsAppRuntimeInstall-x64.exe`, then:

```powershell
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\SuperWin.iss
```

Output: `installer\Output\SuperWin-Setup.exe`.

## Status

Milestone 0 (build scaffolding + tray-resident blank window) is the first target; the
four modules are implemented in the order Volume → Clipboard → Diagnostics → Notepad.
See the task list / plan for progress.
