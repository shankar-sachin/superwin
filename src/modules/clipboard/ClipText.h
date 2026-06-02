// Pure Win32 clipboard read/write helpers (CF_UNICODETEXT). Lives in
// superwin_core so the clipboard watcher (AppHost), the history page and the
// quick picker can all share one implementation -- no WinRT, no UI.
#pragma once

#include <string>

namespace superwin {

// Returns the current clipboard text, or an empty string if there is none.
std::wstring ReadClipboardText();

// Replaces the clipboard contents with `text`.
void WriteClipboardText(const std::wstring& text);

}  // namespace superwin
