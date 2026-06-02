#include "modules/clipboard/ClipText.h"

#include <Windows.h>

#include <cstring>

namespace superwin {

std::wstring ReadClipboardText() {
    if (!::OpenClipboard(nullptr)) return {};
    std::wstring text;
    if (HANDLE h = ::GetClipboardData(CF_UNICODETEXT)) {
        if (auto* p = static_cast<const wchar_t*>(::GlobalLock(h))) {
            text = p;
            ::GlobalUnlock(h);
        }
    }
    ::CloseClipboard();
    return text;
}

void WriteClipboardText(const std::wstring& text) {
    if (!::OpenClipboard(nullptr)) return;
    ::EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL h = ::GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        std::memcpy(::GlobalLock(h), text.c_str(), bytes);
        ::GlobalUnlock(h);
        ::SetClipboardData(CF_UNICODETEXT, h);
    }
    ::CloseClipboard();
}

}  // namespace superwin
