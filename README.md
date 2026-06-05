# SuperWin v2.3.5

A single-process Windows desktop **multi-tool** — a code-first **WinUI 3** (C++/WinRT)
dashboard with a blended, custom title bar, a `NavigationView` shell, and one page per
built-in utility. Sixteen tools (grouped into categories), a global clipboard quick-picker,
a Desmos-style graphing calculator with a built-in CAS, a live system monitor,
self-contained deployment, and automatic updates.

> **Windows 10/11, 64-bit.** Ships self-contained (the Windows App Runtime is bundled),
> so there is **no separate runtime to install**.

![SuperWin demo](media/superwin_demo.gif)

🌐 **Landing page:** <https://shankar-sachin.github.io/superwin/>

---

## Install

**[⬇ Download the latest installer](https://github.com/shankar-sachin/superwin/releases/latest)**
from the GitHub Releases page, or grab a specific build:

- Latest: <https://github.com/shankar-sachin/superwin/releases/latest>
- This version: <https://github.com/shankar-sachin/superwin/releases/download/v2.3.5/SuperWin_v2.3.5.exe>

Then:

1. Run **`SuperWin_v2.3.5.exe`**. It's a **per-user** install — **no UAC / admin prompt**.
2. In the wizard you can optionally **create a desktop shortcut** and **launch SuperWin at
   sign-in**.
3. Launch it. The dashboard opens **maximized**; SuperWin also lives in the system tray and
   runs as a **single instance** (launching it again just re-surfaces the window).

**Updating** is automatic and **in-place**: SuperWin checks for new releases (on launch, or on
demand via **Settings → Check for updates**) and, when a newer version is published, offers to
update. It then downloads a portable build into your user folder, swaps the files, and
**restarts itself** — no installer window and **no admin rights** required.

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
- **JSON Formatter** — pretty-print, minify, and validate JSON (with parse-error messages).
- **GUID Generator** — random version-4 GUIDs, single or in bulk, with uppercase / hyphen /
  brace options.
- **Graphing Calculator** — a **Desmos-style** plotter with a **live math field**: type with
  plain `^ * / sqrt pi` and the input box itself beautifies as you go (`x^2`→`x²`, `*`→`·`, plus
  `√`, `π`, `−`) — the field *is* the pretty equation, no preview line. **Type calculus right
  into the equation** — `d/dx(...)`, `deriv(...)`, `int(...)`, `integral(...)`, `∫(...)dx`, plus
  **summations and products** `sum(x^n/n, 1, 5)` / `prod(...)` (with index `n`); nested and mixed
  expressions too — and it graphs the result, with a numeric fallback when there's no closed form. Multiple colour-coded curves with legend swatches to
  show/hide, drag to pan, scroll or on-canvas **+ / − / home** to zoom, a two-tier grid, and a
  hover **coordinate trace** that snaps to the nearest curve. Backed by a built-in **CAS** that
  evaluates, **differentiates**, **integrates** and **simplifies**, plus a numeric
  **definite-integral** panel (Simpson). Supports `+ - * / ^`, parentheses, `x`, the constants
  `pi`/`e`, and a wide set of functions (trig, hyperbolic, sqrt/cbrt, ln/log, exp, abs, …).
- **Security & Privacy** — cryptographically-secure random tokens (hex/Base64), a password
  **strength meter** (entropy estimate), and one-click clipboard wiping.
- **Always On Top** — assign a **global hotkey** (default `Ctrl+Win+T`) to pin or unpin *any*
  focused window above everything else, PowerToys-style.

Tools are organized into categories — **System, Clipboard & Notepad, Developer, Math,
Security & Privacy, Media** — in both the navigation pane and the Home page. Throughout the
app, **copy buttons flash "Copied"** when clicked, and **Settings** lets you set
the app theme (light / dark / system), launch-at-sign-in, and clipboard behaviour — including
setting the picker hotkey by **physically pressing the keys** (see below).

---

## What's new

- **2.3.5** — **Graphing Calculator** glow-up: a **live math field** that beautifies what you type
  right in the input box (`x²`, `√`, `·`, `π`, `−`) with no preview line; **type calculus into the
  equation** — `d/dx(...)`, `int(...)`, `∫(...)dx`, and now **summations & products**
  (`sum(x^n/n, 1, 5)`, `prod(...)`) — to graph the result. The plot is prettier and smoother, with
  a hover **coordinate trace**, on-canvas **zoom / home** controls, legend swatches to show/hide
  curves, and a **fixed light-mode** plot (crisp grid/axes on white). The **Clipboard quick-picker**
  is redesigned — larger, each clip in its own card with a one-click **✕ delete**, and **reliable
  click-to-paste**. The **auto-updater** now shows per-release notes and always reopens the app
  even if an update step fails.
- **2.3.2** — **In-place updates**: no installer window, no admin rights — SuperWin downloads a
  portable build, swaps files in your user folder, and restarts. The **CAS** gains symbolic
  **integration** (`∫ dx`) and a live **pretty-math preview**, plus a numeric definite-integral
  panel. (Installs are now strictly per-user so updates can never hit a permission wall.)
- **2.3.1** — The **Graphing Calculator** is now **Desmos-style** (multi-function, pan/zoom,
  labelled grid) with a built-in **CAS** (symbolic `d/dx` + simplify). **Always On Top** is now
  a tool with a **global hotkey** to pin any window. Quick-access tiles keep a fixed size and
  wrap instead of stretching.
- **2.3.0** — Four new tools: **JSON Formatter**, **GUID Generator**, **Graphing Calculator**,
  and **Security & Privacy**. Tools are now **grouped into categories** in the nav pane and on
  Home; the Quick access tiles **snap and fill each row** by window width; and Settings gains an
  **Always on top** option for the main window.
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
installer/appcast.xml       self-update feed (portable-zip enclosure)
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

The app is produced at `build\Release\SuperWin.exe` (with `SuperWin.ico`, the Windows App
Runtime DLLs and a merged `resources.pri` deployed next to it).

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
`VERSIONINFO` resource. SuperWin's built-in updater (`src/core/Updater.cpp`, WinHTTP — no
third-party dependency) polls an *appcast* feed; when a newer `sparkle:version` is published it
prompts the user, then downloads a **portable `.zip`**, swaps the files in the (user-writable)
install folder via a small PowerShell helper, and relaunches. No installer UI, no admin.

**To ship an update, bump the version in all four places (they must match):**

1. `src/Version.h`
2. `installer/SuperWin.iss` (`AppVersion`)
3. `README.md` (header + this section)
4. A new `<item>` in `installer/appcast.xml` whose `<enclosure>` `url` points at the portable zip

Then rebuild and create a GitHub release tagged `v<version>` with **two** assets: the wizard
`SuperWin_v<version>.exe` (for fresh installs) and `SuperWin_v<version>_portable.zip` (the
contents of `build\Release`, which the self-updater downloads).

The appcast URL defaults to `kDefaultAppcastUrl` in `src/core/Updater.cpp`
(`https://raw.githubusercontent.com/shankar-sachin/superwin/master/installer/appcast.xml`) and
is overridable at runtime via the `update.appcastUrl` setting.

---

## Conventions

- Everything lives in namespace `superwin` (UI helpers in `superwin::ui`).
- Compiled with `/W4 /permissive- /utf-8 /MP`, C++20, `UNICODE`, `NOMINMAX`,
  `WIN32_LEAN_AND_MEAN` — code is kept warning-clean and uses wide-char Win32 APIs.
- nlohmann/json is vendored header-only under `third_party/`; C++/WinRT and the Windows App SDK
  come from NuGet only in the WinUI build.
