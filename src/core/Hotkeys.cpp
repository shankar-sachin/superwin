#include "core/Hotkeys.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace superwin {
namespace {

UINT VkFromToken(const std::string& token) {
    if (token.size() == 1) {
        const char c = static_cast<char>(std::toupper(token[0]));
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return static_cast<UINT>(c);
    }
    // A few named keys worth supporting in user-facing settings.
    std::string up;
    std::transform(token.begin(), token.end(), std::back_inserter(up),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (up == "SPACE") return VK_SPACE;
    if (up == "INSERT" || up == "INS") return VK_INSERT;
    if (up == "TAB") return VK_TAB;
    if (up.size() >= 2 && up[0] == 'F') {
        const int n = std::atoi(up.c_str() + 1);
        if (n >= 1 && n <= 24) return static_cast<UINT>(VK_F1 + (n - 1));
    }
    return 0;
}

}  // namespace

HotkeyCombo ParseHotkey(const std::string& text) {
    HotkeyCombo combo;
    std::stringstream ss(text);
    std::string token;
    while (std::getline(ss, token, '+')) {
        // trim whitespace
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        std::string up;
        std::transform(token.begin(), token.end(), std::back_inserter(up),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (up == "WIN")        combo.modifiers |= MOD_WIN;
        else if (up == "CTRL" || up == "CONTROL") combo.modifiers |= MOD_CONTROL;
        else if (up == "ALT")   combo.modifiers |= MOD_ALT;
        else if (up == "SHIFT") combo.modifiers |= MOD_SHIFT;
        else                    combo.vk = VkFromToken(token);
    }
    return combo;
}

std::string FormatHotkey(const HotkeyCombo& combo) {
    std::vector<std::string> parts;
    if (combo.modifiers & MOD_WIN)     parts.emplace_back("Win");
    if (combo.modifiers & MOD_CONTROL) parts.emplace_back("Ctrl");
    if (combo.modifiers & MOD_ALT)     parts.emplace_back("Alt");
    if (combo.modifiers & MOD_SHIFT)   parts.emplace_back("Shift");
    if (combo.vk >= 'A' && combo.vk <= 'Z') parts.emplace_back(std::string(1, static_cast<char>(combo.vk)));
    else if (combo.vk >= '0' && combo.vk <= '9') parts.emplace_back(std::string(1, static_cast<char>(combo.vk)));
    else if (combo.vk >= VK_F1 && combo.vk <= VK_F24) parts.emplace_back("F" + std::to_string(combo.vk - VK_F1 + 1));
    else if (combo.vk == VK_SPACE) parts.emplace_back("Space");

    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += '+';
        out += parts[i];
    }
    return out;
}

HotkeyManager::~HotkeyManager() {
    for (auto& [name, entry] : byName_) {
        ::UnregisterHotKey(owner_, entry.id);
    }
}

bool HotkeyManager::Register(const std::string& name, const HotkeyCombo& combo, Callback cb) {
    if (!combo.IsValid()) return false;
    Unregister(name);  // replace any prior binding under this name

    const int id = nextId_++;
    if (!::RegisterHotKey(owner_, id, combo.modifiers | MOD_NOREPEAT, combo.vk)) {
        return false;  // taken by another app (e.g. Win+V is reserved by the shell)
    }
    byName_[name] = Entry{id, combo, cb};
    byId_[id] = std::move(cb);
    return true;
}

void HotkeyManager::Unregister(const std::string& name) {
    const auto it = byName_.find(name);
    if (it == byName_.end()) return;
    ::UnregisterHotKey(owner_, it->second.id);
    byId_.erase(it->second.id);
    byName_.erase(it);
}

bool HotkeyManager::Dispatch(int hotkeyId) {
    const auto it = byId_.find(hotkeyId);
    if (it == byId_.end()) return false;
    if (it->second) it->second();
    return true;
}

}  // namespace superwin
