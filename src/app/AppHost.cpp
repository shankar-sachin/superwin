#include "app/AppHost.h"

#include "core/Hotkeys.h"
#include "core/Settings.h"
#include "core/Strings.h"
#include "modules/clipboard/ClipPicker.h"
#include "modules/clipboard/ClipStore.h"
#include "modules/clipboard/ClipText.h"

namespace superwin {
namespace {

constexpr wchar_t kHostClass[] = L"SuperWin.WinUIHost";

}  // namespace

AppHost::AppHost() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &AppHost::WndProc;
    wc.hInstance = ::GetModuleHandleW(nullptr);
    wc.lpszClassName = kHostClass;
    ::RegisterClassExW(&wc);

    // Normal overlapped window, intentionally never shown. Created on the UI
    // thread so WM_HOTKEY / WM_CLIPBOARDUPDATE callbacks can touch XAML directly.
    hwnd_ = ::CreateWindowExW(0, kHostClass, L"SuperWin", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                              nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd_) return;
    ::SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    picker_ = std::make_unique<ClipPicker>();
    hotkeys_ = std::make_unique<HotkeyManager>(hwnd_);
    ReRegisterHotkey();
    ReRegisterAlwaysOnTopHotkey();

    ::AddClipboardFormatListener(hwnd_);
}

AppHost::~AppHost() {
    if (hwnd_) {
        ::RemoveClipboardFormatListener(hwnd_);
        hotkeys_.reset();  // unregisters before the owner window dies
        ::DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

bool AppHost::ReRegisterHotkey() {
    if (!hotkeys_) return false;
    const std::string combo =
        Settings::Instance().GetString("clipboard.hotkey", "Win+Shift+V");
    hotkeyActive_ = hotkeys_->Register("clipboard", ParseHotkey(combo), [this] { OnHotkey(); });
    return hotkeyActive_;
}

void AppHost::OnHotkey() {
    // Capture the app that had focus *before* the picker steals it, so the
    // picker can paste back into it.
    HWND previous = ::GetForegroundWindow();
    if (picker_) picker_->Show(previous);
}

bool AppHost::ReRegisterAlwaysOnTopHotkey() {
    if (!hotkeys_) return false;
    const std::string combo =
        Settings::Instance().GetString("alwaysOnTop.hotkey", "Ctrl+Win+T");
    aotHotkeyActive_ = hotkeys_->Register("alwaysontop", ParseHotkey(combo), [this] { OnAlwaysOnTopHotkey(); });
    return aotHotkeyActive_;
}

void AppHost::OnAlwaysOnTopHotkey() {
    // Toggle the always-on-top state of whatever window currently has focus.
    HWND target = ::GetForegroundWindow();
    if (!target) return;
    const bool pinned = pinned_.count(target) != 0;
    ::SetWindowPos(target, pinned ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (pinned) pinned_.erase(target); else pinned_.insert(target);
    ::MessageBeep(MB_OK);  // light audible confirmation
}

void AppHost::OnClipboardUpdate() {
    if (!Settings::Instance().GetBool("clipboard.monitor", true)) return;
    std::wstring text = ReadClipboardText();
    if (!text.empty()) SharedClipStore().AddText(WideToUtf8(text));
}

LRESULT CALLBACK AppHost::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<AppHost*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) {
        switch (msg) {
            case WM_HOTKEY:
                if (self->hotkeys_ && self->hotkeys_->Dispatch(static_cast<int>(wParam))) return 0;
                break;
            case WM_CLIPBOARDUPDATE:
                self->OnClipboardUpdate();
                return 0;
            default:
                break;
        }
    }
    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace superwin
