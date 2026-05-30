// Small UTF-8 <-> UTF-16 helpers used throughout SuperWin.
// The Windows APIs are wide (UTF-16); our settings/JSON layer is UTF-8.
#pragma once

#include <string>
#include <string_view>
#include <Windows.h>

namespace superwin {

inline std::wstring Utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int len = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                          static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring wide(static_cast<size_t>(len), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                          wide.data(), len);
    return wide;
}

inline std::string WideToUtf8(std::wstring_view wide) {
    if (wide.empty()) return {};
    const int len = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                          static_cast<int>(wide.size()),
                                          nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<size_t>(len), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                          utf8.data(), len, nullptr, nullptr);
    return utf8;
}

}  // namespace superwin
