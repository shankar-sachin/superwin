// Auto-update support via WinSparkle (https://winsparkle.org).
//
// SuperWin ships as an unpackaged app installed by an Inno Setup wizard, so we
// use the Sparkle model: the app periodically fetches an "appcast" XML feed; when
// it advertises a version newer than this build, WinSparkle prompts the user,
// downloads the new SuperWin-Setup.exe, and runs it to upgrade in place.
//
// The appcast URL is read from settings ("update.appcastUrl") so it can be
// repointed without a rebuild; otherwise it falls back to the compiled default.
#pragma once

namespace superwin {

class Updater {
public:
    // Configure WinSparkle (appcast URL, app identity, current version) and start
    // the background checker. Call once at startup, after the message loop owner
    // exists. No-op-safe to call again.
    static void Initialize();

    // Manually trigger a check that shows UI (wire to a "Check for updates" menu).
    static void CheckNow();

    // Stop WinSparkle's worker thread cleanly before exit.
    static void Shutdown();
};

}  // namespace superwin
