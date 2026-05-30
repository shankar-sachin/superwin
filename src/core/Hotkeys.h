// System-wide hotkey registration on a single owner window.
//
// Each hotkey is registered with RegisterHotKey against a hidden message
// window; WM_HOTKEY is routed back here via Dispatch(). Combinations are
// described by a small struct so they can be loaded from / saved to Settings.
#pragma once

#include <Windows.h>

#include <functional>
#include <string>
#include <unordered_map>

namespace superwin {

struct HotkeyCombo {
    UINT modifiers = 0;  // MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN (no MOD_NOREPEAT)
    UINT vk = 0;         // virtual-key code, e.g. 'V'

    bool IsValid() const { return vk != 0; }
};

// Parse / format combos like "Win+Shift+V" for storage in settings.
HotkeyCombo ParseHotkey(const std::string& text);
std::string FormatHotkey(const HotkeyCombo& combo);

class HotkeyManager {
public:
    using Callback = std::function<void()>;

    explicit HotkeyManager(HWND owner) : owner_(owner) {}
    ~HotkeyManager();

    // Register `combo`, invoking `cb` when pressed. Returns false if the
    // combination is already held by another application (RegisterHotKey fail).
    // Re-registering the same `name` replaces the previous binding.
    bool Register(const std::string& name, const HotkeyCombo& combo, Callback cb);
    void Unregister(const std::string& name);

    // Call from the owner window's WM_HOTKEY handler. Returns true if handled.
    bool Dispatch(int hotkeyId);

private:
    struct Entry {
        int id;
        HotkeyCombo combo;
        Callback cb;
    };

    HWND owner_;
    int nextId_ = 0xB000;  // arbitrary base to avoid clashing with menu ids
    std::unordered_map<std::string, Entry> byName_;
    std::unordered_map<int, Callback> byId_;
};

}  // namespace superwin
