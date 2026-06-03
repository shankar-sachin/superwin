# SuperWin v2.1.4

A single-process Windows desktop **multi-tool** — a code-first **WinUI 3** (C++/WinRT)
dashboard with a blended, custom title bar, a `NavigationView` shell, and one page per
built-in utility. Eleven tools, a global clipboard quick-picker, a live system monitor,
self-contained deployment, and automatic updates.

> **Windows 10/11, 64-bit.** Ships self-contained (the Windows App Runtime is bundled),
> so there is **no separate runtime to install**.

---

## Install

**[⬇ Download the latest installer](https://github.com/shankar-sachin/superwin/releases/latest)**
from the GitHub Releases page, or grab a specific build:

- Latest: <https://github.com/shankar-sachin/superwin/releases/latest>
- This version: <https://github.com/shankar-sachin/superwin/releases/download/v2.1.4/SuperWin_v2.1.4.exe>

Then:

1. Run **`SuperWin_v2.1.4.exe`**. It's a **per-user** install — **no UAC / admin prompt**.
2. In the wizard you can optionally **create a desktop shortcut** and **launch SuperWin at
   sign-in**.
3. Launch it. The dashboard opens **maximized**; SuperWin also lives in the system tray and
   runs as a **single instance** (launching it again just re-surfaces the window).

**Updating** is automatic: SuperWin checks for new releases via **WinSparkle** (every 24h, or
on demand via **Settings → Check for updates**) and, when a newer version is published, offers
to download and install it in place.

**Uninstall** from *Settings → Apps* (or *Add/Remove Programs*); per-user data in
`%APPDATA%\SuperWin` is removed with it.

---

## Tools

- **Volume Customizer** — per-app and master volume in exact **percent and decibels** (WASAPI).
- **Clipboard** — **process-wide** clipboard history with a global quick-picker
  (**`Win+Shift+V`** by default): search-as-you-type, **pinned** items, full keyboard
  navigation, and optional **auto-paste** straight back into the app you were using. History
  is captured for the whole session whether or not the Clipboard page is open.
- **Diagnostics + Mini-monitor** — full system inventory plus live performance:
  - **System facts:** CPU (name, vendor, architecture, physical cores / logical threads, base
    clock), all **GPUs** + VRAM, total RAM, **OS name & build** (correctly reports **Windows 11**),
    architecture, device & signed-in user name, manufacturer/model, BIOS, motherboard, every
    fixed **drive** (free/total + filesystem), and display count/primary resolution.
  - **Live:** CPU %, RAM % (used/total), **GPU utilization & VRAM**, **disk activity % and
    read/write throughput** (the same PDH counters Task Manager uses), committed & cached
    memory, process/thread/handle counts, and uptime.
  - **Mini-monitor:** an always-on-top window showing colour-coded **CPU / RAM / GPU / Disk**
    with bold values and slim bars. It's **DPI-aware** (fits all four at any display scale) and
    **keeps updating after you navigate away** from the Diagnostics page.
- **Notepad Super** — a "Word-Lite" rich-text editor (WinUI `RichEditBox`): font family & size,
  **bold / italic / underline / strikethrough**, **text & highlight colors** (with a
  **No highlight** option to clear), alignment, bullet/numbered lists, undo/redo, and **RTF /
  TXT** open & save.
- **Color Picker** — full colour wheel, **hex / RGB** with copy, and a screen eyedropper.
- **Keep Awake** — stop sleep / screen-off, optionally for a fixed duration.
- **Hash & Checksum** — **MD5 / SHA-1 / SHA-256** of text or files (CNG/BCrypt).
- **Network Info** — adapters, IPv4/IPv6, MAC, and a quick ping.
- **Unit Converter** — length / mass / temperature / data-size units, plus number bases.
- **Password Generator** — configurable length and character classes with a strength readout.
- **Text** — case conversion, trimming, counting, and other quick text transforms.

Throughout the app, **copy buttons flash "Copied"** when clicked, and **Settings** lets you set
the app theme (light / dark / system), launch-at-sign-in, and clipboard behaviour — including
setting the picker hotkey by **physically pressing the keys** (see below).

---

## What's new in 2.1.x

- **2.1.4** — Mini-monitor is now **DPI-aware and sized to its content** (all four metrics
  always fit, at any display scale) and **colour-coded** (CPU blue, RAM green, GPU purple,
  Disk orange).
- **2.1.3** — Mini-monitor redesigned into tidy icon rows with values and slim bars.
- **2.1.2** — **Press-to-set hotkey** in Settings (no more typing the combo); live **GPU/VRAM**
  and **disk activity/throughput** added to Diagnostics; mini-monitor extended to CPU/RAM/GPU/
  Disk and no longer freezes when you leave the page.
- **2.1.1** — Fixed a versioning slip (the 2.1.0 build still reported 2.0.0, which broke update
  detection); fixed **Notepad** colour pickers (no more stray default colour on open), made
  colouring apply reliably to the selection, kept the editor focused after toolbar actions, and
  added **No highlight**; **copy buttons** now show "Copied"; the **`Win+Shift+V` picker** is
  forced reliably to the foreground; the **dashboard opens maximized**; and the **auto-updater**
  now points at the real release feed.
- **2.1.0** — Added the **Password Generator** and **Text** tools.

---

## Architecture

A single `SuperWin.exe` process hosting a code-first WinUI 3 dashboard (`NavigationView`
shell). An `AppHost` owns a hidden Win32 message window on the UI thread that registers the
global clipboard hotkey and listens for OS clipboard changes
(`AddClipboardFormatListener`), feeding a **process-wide** `SharedClipStore` for the whole
session. The UI is built **code-first in C++/WinRT** (no XAML markup files), which keeps the
CMake build free of the XAML compiler.

The codebase is split so that **all testable logic lives outside WinUI**: a `superwin_core`
static library holds pure logic + Win32 infrastructure (no WinUI / C++/WinRT / NuGet) and is
linked by both the app and the Catch2 tests; the WinUI pages are only compiled when the
`SUPERWIN_ENABLE_WINUI` option is on.

```
src/
  main.cpp                  WinMain / WinUI App: bootstrap, single-instance, AppHost
  Version.h                 single source of truth for the version
  app/
    Shell                   NavigationView shell + blended (extended) title bar; opens maximized
    AppHost                 hidden Win32 host: global hotkey + clipboard listener
    HomePage, SettingsPage  dashboard home tiles + Settings (theme, startup, clipboard, hotkey)
    Ui.h                    shared code-first layout helpers (cards, stacks, FlashCopied, …)
  core/                     Settings, SingleInstance, Hotkeys, TrayIcon, Autostart, Updater
  modules/
    volume/                 AudioSessions (WASAPI), VolumeMath, VolumePage
    clipboard/              ClipStore, ClipText, ClipPage, ClipPicker (quick picker)
    diagnostics/            HardwareProbe (registry/DXGI/PDH), DiagPage + mini-monitor
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
installer/appcast.xml       WinSparkle update feed
```

Settings are a process-wide singleton persisted as JSON at `%APPDATA%\SuperWin\settings.json`,
addressed by dotted paths (e.g. `clipboard.hotkey`, `ui.theme`, `update.appcastUrl`).

---

## Build from source

### Prerequisites

- **Visual Studio 2026** with the **Desktop development with C++** workload
  (MSVC toolset + Windows 11 SDK + CMake + Ninja).
- **Inno Setup 6** (only to build the installer) — `winget install JRSoftware.InnoSetup`.

Third-party libraries are pulled automatically via **NuGet `PackageReference`** (wired through
CMake `VS_PACKAGE_REFERENCES`): Windows App SDK, C++/WinRT, WIL, nlohmann.json, and Catch2. No
vcpkg or manual restore step is required.

### Configure & build

From a *Developer Command Prompt for VS 2026* (or any shell with CMake on PATH):

```powershell
cmake --preset msvc -DSUPERWIN_ENABLE_WINUI=ON   # configure with the full WinUI dashboard
cmake --build --preset msvc-release              # build Release (uses /MP across all cores)
```

The app is produced at `build\Release\SuperWin.exe` (with `WinSparkle.dll`, `SuperWin.ico`,
the Windows App Runtime DLLs and a merged `resources.pri` deployed next to it).

Without `-DSUPERWIN_ENABLE_WINUI=ON`, the configure step builds only the testable core + a Win32
tray host, so the logic library and tests compile **without any NuGet restore**.

### Run the tests

```powershell
ctest --preset msvc-release
ctest --preset msvc-release -R ConvertLogic   # a single test by name regex
```

### Build the installer

The app is built **self-contained** (the Windows App Runtime ships inside the app folder), so no
runtime redist staging is required:

```powershell
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" installer\SuperWin.iss
```

Output: `installer\Output\SuperWin_v<version>.exe` — a per-user wizard (no UAC) with a clean
uninstaller.

---

## Versioning & auto-updates

`src/Version.h` is the **single source of truth** for the version; the exe carries a matching
`VERSIONINFO` resource. SuperWin auto-updates via **WinSparkle**: the installed app polls an
*appcast* feed and, when a newer `sparkle:version` is published, prompts the user and upgrades in
place.

**To ship an update, bump the version in all four places (they must match):**

1. `src/Version.h`
2. `installer/SuperWin.iss` (`AppVersion`)
3. `README.md` (header + this section)
4. A new `<item>` in `installer/appcast.xml` (new `sparkle:version` + the release download URL)

Then rebuild, and upload `installer\Output\SuperWin_v<version>.exe` as the asset of a GitHub
release tagged `v<version>`.

The appcast URL defaults to `kDefaultAppcastUrl` in `src/core/Updater.cpp`
(`https://raw.githubusercontent.com/shankar-sachin/superwin/master/installer/appcast.xml`) and
is overridable at runtime via the `update.appcastUrl` setting. (For production, sign updates with
an EdDSA key via `win_sparkle_set_eddsa_*` and add `sparkle:edSignature` to the feed.)

---

## Conventions

- Everything lives in namespace `superwin` (UI helpers in `superwin::ui`).
- Compiled with `/W4 /permissive- /utf-8 /MP`, C++20, `UNICODE`, `NOMINMAX`,
  `WIN32_LEAN_AND_MEAN` — code is kept warning-clean and uses wide-char Win32 APIs.
- nlohmann/json is vendored header-only under `third_party/`; C++/WinRT and the Windows App SDK
  come from NuGet only in the WinUI build.
