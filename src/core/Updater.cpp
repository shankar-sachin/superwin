#include "core/Updater.h"

#include <winsparkle.h>

#include <string>

#include "Version.h"
#include "core/Settings.h"
#include "core/Strings.h"

namespace superwin {
namespace {

// Default appcast location. Repoint this (or override "update.appcastUrl" in
// settings) at wherever you publish releases, e.g. a GitHub Pages URL or a
// "latest" release asset. See installer/appcast.xml for the feed format.
constexpr char kDefaultAppcastUrl[] =
    "https://raw.githubusercontent.com/your-org/superwin/main/appcast.xml";

bool g_initialized = false;

}  // namespace

void Updater::Initialize() {
    if (g_initialized) return;
    g_initialized = true;

    auto& settings = Settings::Instance();
    const std::string url = settings.GetString("update.appcastUrl", kDefaultAppcastUrl);

    win_sparkle_set_appcast_url(url.c_str());
    win_sparkle_set_app_details(L"SuperWin", L"SuperWin",
                                Utf8ToWide(SUPERWIN_VERSION_STRING).c_str());

    // Check on a schedule (every 24h by default); WinSparkle persists the user's
    // "automatically check" preference after the first prompt.
    win_sparkle_set_automatic_check_for_updates(
        settings.GetBool("update.autoCheck", true) ? 1 : 0);
    win_sparkle_set_update_check_interval(60 * 60 * 24);  // seconds

    win_sparkle_init();
}

void Updater::CheckNow() {
    if (!g_initialized) Initialize();
    win_sparkle_check_update_with_ui();
}

void Updater::Shutdown() {
    if (!g_initialized) return;
    win_sparkle_cleanup();
    g_initialized = false;
}

}  // namespace superwin
