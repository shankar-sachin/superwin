// In-app self-updater (WinHTTP, no external dependency).
//
// SuperWin installs per-user, so updating needs no admin rights: the app fetches
// a Sparkle-format "appcast" XML feed, and when it advertises a version newer
// than this build, a helper script downloads the portable .zip, swaps the app
// folder in place, and relaunches SuperWin (see core/Updater.cpp).
//
// The appcast URL is read from settings ("update.appcastUrl") so it can be
// repointed without a rebuild; otherwise it falls back to the compiled default.
#pragma once

namespace superwin {

class Updater {
public:
    // Start the quiet background update check. Call once at startup, after the
    // message loop owner exists. No-op-safe to call again.
    static void Initialize();

    // Manually trigger a check that shows UI (wire to a "Check for updates" menu).
    static void CheckNow();

    // No-op (kept for symmetry with Initialize at the call sites).
    static void Shutdown();
};

}  // namespace superwin
