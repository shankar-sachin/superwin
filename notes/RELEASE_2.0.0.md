# SuperWin 2.0.0

*Released 2026-06-01*

A big release that polishes the whole app, fixes the Clipboard tool end to end, adds
a Settings page, and ships four new utilities.

## Highlights

### Blended title bar
The window's title bar is now extended into the app — the Mica surface flows up behind
the minimize / restore / close buttons (drawn transparently), with the app icon and
name on the left. No more disconnected system caption strip; the whole window reads as
one surface.

### Clipboard (formerly "Clipboard++") — fixed end to end
The clipboard tool was renamed to **Clipboard** and reworked so it actually works:

- **History is captured process-wide.** A background clipboard listener runs for the
  whole session, so anything you copy is saved — you no longer have to keep the
  Clipboard page open for history to fill up.
- **`Win+Shift+V` now works.** It opens a quick picker right at your cursor:
  - **Search as you type** to filter the history.
  - **Pinned items first**, so your favorites stay on top.
  - **Full keyboard navigation** — type to filter, ↑/↓ to move, **Enter** to choose,
    **Esc** to dismiss; it also auto-dismisses when it loses focus.
  - **Auto-paste** — picking a clip pastes it straight back into the app you were
    using (toggle off in Settings if an app misbehaves).
- The Clipboard page live-refreshes, with copy / pin / delete per item.

### New Settings page
A proper Settings section in the navigation pane:

- **App theme** — Light, Dark, or follow Windows.
- **Launch at sign-in** toggle.
- **Clipboard** — track-history toggle, auto-paste toggle, history size, the picker
  hotkey (rebindable, with a live status indicator), and a clear-history button.
- **About** — version and a Check-for-updates button.

### Four new tools
- **Keep Awake** — stop your PC from sleeping / blanking the screen, optionally for a
  fixed duration (15 min / 30 min / 1 hr / 2 hr) with a live countdown.
- **Hash & Checksum** — MD5 / SHA-1 / SHA-256 of typed text or a chosen file, with
  one-click copy. Backed by the Windows CNG (BCrypt) API.
- **Network Info** — list adapters with IPv4 / IPv6 / MAC and up/down status, plus a
  quick ping tool.
- **Unit Converter** — length, mass, temperature, and data-size unit conversions, plus
  number-base conversion (decimal / hex / binary / octal).

## Full tool set
Volume Customizer · Clipboard · Diagnostics (+ mini-monitor) · Notepad Super ·
Color Picker · Keep Awake · Hash & Checksum · Network Info · Unit Converter.

## Notes & known behavior
- **Auto-paste** synthesizes `Ctrl+V` into the previously focused window. A few apps
  with non-standard paste handling may not accept it — turn auto-paste off in Settings
  to fall back to copy-only (the clip is still placed on the clipboard).
- The clipboard picker positions itself at the cursor and is clamped to the current
  monitor's work area.

## Install
Run **`SuperWin-Setup.exe`**. It's a per-user install (no UAC prompt) into Program
Files, with an optional "launch at sign-in" task and desktop shortcut, and a clean
uninstaller. The app is self-contained — no separate runtime download is required.

## Upgrading
Installed copies update via WinSparkle: SuperWin polls its appcast feed (every 24h, or
on demand via **Settings → Check for updates**) and, when a newer version is published,
prompts to download and install it in place.

## For maintainers
- Version is single-sourced in `src/Version.h` (matching `VERSIONINFO` in the exe);
  also bumped in `installer/SuperWin.iss` and `installer/appcast.xml`.
- Build: `cmake --preset msvc` then `cmake --build build --config Release --target SuperWin`.
- Tests: `cmake --build build --config Release --target SuperWin_tests` then run
  `build\Release\SuperWin_tests.exe` (Catch2).
- Installer: `ISCC.exe installer\SuperWin.iss` → `installer\Output\SuperWin-Setup.exe`.
