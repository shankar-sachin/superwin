// Process-wide Win32 backbone for the WinUI app.
//
// The WinUI dashboard window is owned by the framework, so it is not a good
// place to hang global hotkeys or a clipboard listener. AppHost owns a small
// hidden message window (created on the UI thread) that:
//   * registers the Win+Shift+V global hotkey and opens the clipboard picker,
//   * listens for OS clipboard changes (AddClipboardFormatListener) and feeds
//     captured text into the process-wide SharedClipStore for the whole app
//     lifetime -- not just while the Clipboard page is open.
//
// This mirrors the Stage 0b Win32 host in main.cpp, ported into the WinUI path.
#pragma once

#include <Windows.h>

#include <memory>

namespace superwin {

class HotkeyManager;
class ClipPicker;

class AppHost {
public:
    AppHost();
    ~AppHost();

    AppHost(const AppHost&) = delete;
    AppHost& operator=(const AppHost&) = delete;

    HWND Hwnd() const { return hwnd_; }

    // Re-read clipboard.hotkey from Settings and (re)register it. Returns false
    // if the combination is rejected (already owned by another app).
    bool ReRegisterHotkey();

    // Whether the last hotkey (re)registration succeeded.
    bool HotkeyActive() const { return hotkeyActive_; }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void OnHotkey();
    void OnClipboardUpdate();

    HWND                           hwnd_ = nullptr;
    std::unique_ptr<HotkeyManager> hotkeys_;
    std::unique_ptr<ClipPicker>    picker_;
    bool                           hotkeyActive_ = false;
};

}  // namespace superwin
