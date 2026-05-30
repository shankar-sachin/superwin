// Toggle "launch SuperWin at sign-in" via the per-user Run key. Per-user (HKCU)
// needs no elevation, matching an Inno Setup install into the user profile or a
// normal Program Files install launched by the user.
#pragma once

namespace superwin {

class Autostart {
public:
    // Whether the Run entry currently points at this executable.
    static bool IsEnabled();

    // Add / remove the Run entry for the current executable path.
    // Returns true on success.
    static bool SetEnabled(bool enabled);

private:
    static constexpr const wchar_t* kRunKey =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static constexpr const wchar_t* kValueName = L"SuperWin";
};

}  // namespace superwin
