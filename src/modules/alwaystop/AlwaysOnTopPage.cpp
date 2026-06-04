// Always On Top tool: assign a global hotkey that pins (or unpins) whatever
// window currently has focus -- like PowerToys' Always On Top. The hotkey is
// owned by AppHost; this page just lets you set it by pressing the keys.
#include <Windows.h>

#include <memory>
#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>

#include "app/AppHost.h"
#include "app/Shell.h"
#include "app/Ui.h"
#include "core/Hotkeys.h"
#include "core/Settings.h"
#include "core/Strings.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {

std::unique_ptr<IModulePage> MakeAlwaysOnTopPage(AppHost* host) {
    auto& settings = Settings::Instance();
    auto body = ui::VStack(14);

    {
        auto intro = ui::VStack(4);
        intro.Children().Append(ui::Text(L"Pin any window on top", 15, true));
        intro.Children().Append(ui::Caption(
            L"Focus any window, then press your hotkey to keep it above everything else. "
            L"Press it again on the same window to unpin. A short beep confirms each toggle."));
        body.Children().Append(ui::Card(intro));
    }

    auto status = ui::Caption(host && host->AlwaysOnTopHotkeyActive()
        ? L"Active" : L"Not registered \x2014 may be in use by another app.");

    auto hotkeyBox = winrt::TextBox();
    hotkeyBox.IsReadOnly(true);
    hotkeyBox.Width(200);
    hotkeyBox.PlaceholderText(L"Press a shortcut\x2026");
    hotkeyBox.Text(winrt::hstring(Utf8ToWide(settings.GetString("alwaysOnTop.hotkey", "Ctrl+Win+T"))));

    auto recording = std::make_shared<bool>(false);

    hotkeyBox.GotFocus([hotkeyBox, recording, status](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
        *recording = true;
        hotkeyBox.Text(L"Press a shortcut\x2026");
        status.Text(L"Listening\x2026 press your key combination.");
    });
    hotkeyBox.LostFocus([hotkeyBox, recording, host, status](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
        *recording = false;
        hotkeyBox.Text(winrt::hstring(Utf8ToWide(
            Settings::Instance().GetString("alwaysOnTop.hotkey", "Ctrl+Win+T"))));
        if (host) status.Text(host->AlwaysOnTopHotkeyActive()
            ? L"Active" : L"Not registered \x2014 try another combination.");
    });
    hotkeyBox.KeyDown([hotkeyBox, recording, host, status](
            winrt::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e) {
        if (!*recording) return;
        e.Handled(true);
        using VK = winrt::Windows::System::VirtualKey;
        const VK key = e.Key();
        if (key == VK::Control || key == VK::Shift || key == VK::Menu ||
            key == VK::LeftWindows || key == VK::RightWindows ||
            key == VK::LeftControl || key == VK::RightControl ||
            key == VK::LeftShift || key == VK::RightShift ||
            key == VK::LeftMenu || key == VK::RightMenu) {
            return;
        }
        const UINT vk = static_cast<UINT>(key);
        const bool mainKey = (vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9') ||
                             (vk >= VK_F1 && vk <= VK_F24) || vk == VK_SPACE;
        if (!mainKey) { status.Text(L"Use a letter, number, F-key, or Space."); return; }

        HotkeyCombo combo;
        if (::GetKeyState(VK_CONTROL) & 0x8000) combo.modifiers |= MOD_CONTROL;
        if (::GetKeyState(VK_SHIFT)   & 0x8000) combo.modifiers |= MOD_SHIFT;
        if (::GetKeyState(VK_MENU)    & 0x8000) combo.modifiers |= MOD_ALT;
        if ((::GetKeyState(VK_LWIN) & 0x8000) || (::GetKeyState(VK_RWIN) & 0x8000)) combo.modifiers |= MOD_WIN;
        combo.vk = vk;
        if (combo.modifiers == 0) { status.Text(L"Add a modifier: Ctrl, Alt, Shift, or Win."); return; }

        const std::string formatted = FormatHotkey(combo);
        Settings::Instance().Set("alwaysOnTop.hotkey", formatted);
        const bool ok = host && host->ReRegisterAlwaysOnTopHotkey();
        hotkeyBox.Text(winrt::hstring(Utf8ToWide(formatted)));
        status.Text(ok ? L"Active" : L"Not registered \x2014 try another combination.");
    });

    auto row = ui::HStack(8);
    row.VerticalAlignment(winrt::VerticalAlignment::Center);
    row.Children().Append(hotkeyBox);
    row.Children().Append(status);

    auto card = ui::VStack(8);
    card.Children().Append(ui::Text(L"Pin / unpin hotkey", 14, true));
    card.Children().Append(ui::Caption(L"Click the box, then press the keys you want."));
    card.Children().Append(row);
    body.Children().Append(ui::Card(card));

    return std::make_unique<SimplePage>(ui::Page(L"Always On Top", body));
}

}  // namespace superwin
