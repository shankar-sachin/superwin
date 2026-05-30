#include "core/Autostart.h"

#include <Windows.h>

#include <string>

namespace superwin {
namespace {

std::wstring ExecutablePath() {
    wchar_t buf[MAX_PATH];
    const DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::wstring(buf, n);
}

// Stored value is quoted + a startup flag so the app can start minimized.
std::wstring QuotedCommand() {
    return L"\"" + ExecutablePath() + L"\" --autostart";
}

}  // namespace

bool Autostart::IsEnabled() {
    HKEY key;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return false;

    wchar_t buf[1024];
    DWORD size = sizeof(buf);
    DWORD type = 0;
    const LONG r = ::RegQueryValueExW(key, kValueName, nullptr, &type,
                                      reinterpret_cast<LPBYTE>(buf), &size);
    ::RegCloseKey(key);
    if (r != ERROR_SUCCESS || type != REG_SZ) return false;

    return _wcsicmp(buf, QuotedCommand().c_str()) == 0;
}

bool Autostart::SetEnabled(bool enabled) {
    HKEY key;
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
                          KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;

    LONG r;
    if (enabled) {
        const std::wstring cmd = QuotedCommand();
        r = ::RegSetValueExW(key, kValueName, 0, REG_SZ,
                             reinterpret_cast<const BYTE*>(cmd.c_str()),
                             static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        r = ::RegDeleteValueW(key, kValueName);
        if (r == ERROR_FILE_NOT_FOUND) r = ERROR_SUCCESS;  // already absent
    }
    ::RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

}  // namespace superwin
