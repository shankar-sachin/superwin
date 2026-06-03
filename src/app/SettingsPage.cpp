#include <Windows.h>

#include <memory>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>

#include "Version.h"
#include "app/AppHost.h"
#include "app/Shell.h"
#include "app/Ui.h"
#include "core/Autostart.h"
#include "core/Hotkeys.h"
#include "core/Settings.h"
#include "core/Strings.h"
#include "core/Updater.h"
#include "modules/clipboard/ClipStore.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

// A labelled settings row: a title + caption on the left, a control on the right.
winrt::Border SettingRow(winrt::hstring title, winrt::hstring caption, winrt::UIElement control) {
    auto labels = ui::VStack(2);
    labels.VerticalAlignment(winrt::VerticalAlignment::Center);
    labels.Children().Append(ui::Text(title, 14, true));
    if (!caption.empty()) labels.Children().Append(ui::Caption(caption));

    auto row = ui::HStack(16);
    row.VerticalAlignment(winrt::VerticalAlignment::Center);
    labels.HorizontalAlignment(winrt::HorizontalAlignment::Left);
    labels.Width(360);
    row.Children().Append(labels);
    control.as<winrt::FrameworkElement>().VerticalAlignment(winrt::VerticalAlignment::Center);
    row.Children().Append(control);
    return ui::Card(row, 14);
}

winrt::TextBlock SectionHeader(winrt::hstring s) {
    return ui::Text(s, 16, true);
}

}  // namespace

std::unique_ptr<IModulePage> MakeSettingsPage(Shell* shell, AppHost* host) {
    auto& settings = Settings::Instance();
    auto body = ui::VStack(14);

    // ---- Appearance ----
    body.Children().Append(SectionHeader(L"Appearance"));
    {
        auto combo = winrt::ComboBox();
        combo.Items().Append(winrt::box_value(winrt::hstring(L"Use system setting")));
        combo.Items().Append(winrt::box_value(winrt::hstring(L"Light")));
        combo.Items().Append(winrt::box_value(winrt::hstring(L"Dark")));
        const std::string theme = settings.GetString("ui.theme", "system");
        combo.SelectedIndex(theme == "light" ? 1 : theme == "dark" ? 2 : 0);
        combo.SelectionChanged([shell](winrt::IInspectable const& s, winrt::IInspectable const&) {
            const int i = s.as<winrt::ComboBox>().SelectedIndex();
            const char* mode = i == 1 ? "light" : i == 2 ? "dark" : "system";
            Settings::Instance().Set("ui.theme", std::string_view(mode));
            if (shell) shell->SetTheme(winrt::hstring(Utf8ToWide(mode)));
        });
        body.Children().Append(SettingRow(L"App theme", L"Choose light, dark, or follow Windows.", combo));
    }

    // ---- Startup ----
    body.Children().Append(SectionHeader(L"Startup"));
    {
        auto toggle = winrt::ToggleSwitch();
        toggle.IsOn(Autostart::IsEnabled());
        toggle.Toggled([](winrt::IInspectable const& s, winrt::RoutedEventArgs const&) {
            Autostart::SetEnabled(s.as<winrt::ToggleSwitch>().IsOn());
        });
        body.Children().Append(SettingRow(L"Launch at sign-in", L"Start SuperWin automatically when you log in.", toggle));
    }

    // ---- Clipboard ----
    body.Children().Append(SectionHeader(L"Clipboard"));
    {
        auto monitor = winrt::ToggleSwitch();
        monitor.IsOn(settings.GetBool("clipboard.monitor", true));
        monitor.Toggled([](winrt::IInspectable const& s, winrt::RoutedEventArgs const&) {
            Settings::Instance().Set("clipboard.monitor", s.as<winrt::ToggleSwitch>().IsOn());
        });
        body.Children().Append(SettingRow(L"Track clipboard history", L"Capture copied text into the history.", monitor));

        auto autoPaste = winrt::ToggleSwitch();
        autoPaste.IsOn(settings.GetBool("clipboard.autoPaste", true));
        autoPaste.Toggled([](winrt::IInspectable const& s, winrt::RoutedEventArgs const&) {
            Settings::Instance().Set("clipboard.autoPaste", s.as<winrt::ToggleSwitch>().IsOn());
        });
        body.Children().Append(SettingRow(L"Auto-paste from picker", L"Paste straight into the app after you pick a clip.", autoPaste));

        auto maxItems = winrt::NumberBox();
        maxItems.Minimum(1);
        maxItems.Maximum(500);
        maxItems.SmallChange(5);
        maxItems.SpinButtonPlacementMode(winrt::NumberBoxSpinButtonPlacementMode::Inline);
        maxItems.Value(settings.GetInt("clipboard.maxItems", 50));
        maxItems.ValueChanged([](winrt::NumberBox const& nb, winrt::NumberBoxValueChangedEventArgs const&) {
            const int n = static_cast<int>(nb.Value());
            if (n < 1) return;
            Settings::Instance().Set("clipboard.maxItems", n);
            SharedClipStore().SetMaxItems(n);
        });
        body.Children().Append(SettingRow(L"History size", L"How many unpinned clips to keep.", maxItems));

        // Hotkey: click the box and physically press the combination. The
        // capture applies (and re-registers) immediately -- no typing, no Apply.
        auto status = ui::Caption(host && host->HotkeyActive()
            ? L"Active" : L"Not registered \x2014 may be in use by another app.");
        auto hotkeyBox = winrt::TextBox();
        hotkeyBox.IsReadOnly(true);
        hotkeyBox.Width(180);
        hotkeyBox.PlaceholderText(L"Press a shortcut\x2026");
        hotkeyBox.Text(winrt::hstring(Utf8ToWide(settings.GetString("clipboard.hotkey", "Win+Shift+V"))));

        auto recording = std::make_shared<bool>(false);

        hotkeyBox.GotFocus([hotkeyBox, recording, status](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            *recording = true;
            hotkeyBox.Text(L"Press a shortcut\x2026");
            status.Text(L"Listening\x2026 press your key combination.");
        });
        hotkeyBox.LostFocus([hotkeyBox, recording, host, status](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            *recording = false;
            hotkeyBox.Text(winrt::hstring(Utf8ToWide(
                Settings::Instance().GetString("clipboard.hotkey", "Win+Shift+V"))));
            if (host) status.Text(host->HotkeyActive()
                ? L"Active" : L"Not registered \x2014 try another combination.");
        });
        hotkeyBox.KeyDown([hotkeyBox, recording, host, status](
                winrt::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e) {
            if (!*recording) return;
            e.Handled(true);  // swallow navigation/beeps while capturing
            using VK = winrt::Windows::System::VirtualKey;
            const VK key = e.Key();
            // Ignore presses of a modifier on its own -- wait for the real key.
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
            if (!mainKey) {
                status.Text(L"Use a letter, number, F-key, or Space.");
                return;
            }
            HotkeyCombo combo;
            if (::GetKeyState(VK_CONTROL) & 0x8000) combo.modifiers |= MOD_CONTROL;
            if (::GetKeyState(VK_SHIFT)   & 0x8000) combo.modifiers |= MOD_SHIFT;
            if (::GetKeyState(VK_MENU)    & 0x8000) combo.modifiers |= MOD_ALT;
            if ((::GetKeyState(VK_LWIN) & 0x8000) || (::GetKeyState(VK_RWIN) & 0x8000))
                combo.modifiers |= MOD_WIN;
            combo.vk = vk;
            if (combo.modifiers == 0) {
                status.Text(L"Add a modifier: Ctrl, Alt, Shift, or Win.");
                return;
            }
            const std::string formatted = FormatHotkey(combo);
            Settings::Instance().Set("clipboard.hotkey", formatted);
            const bool ok = host && host->ReRegisterHotkey();
            hotkeyBox.Text(winrt::hstring(Utf8ToWide(formatted)));
            status.Text(ok ? L"Active" : L"Not registered \x2014 try another combination.");
        });

        auto hkRow = ui::HStack(8);
        hkRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        hkRow.Children().Append(hotkeyBox);
        hkRow.Children().Append(status);
        body.Children().Append(SettingRow(L"Picker hotkey",
            L"Click the box, then press the keys you want.", hkRow));

        auto clear = winrt::Button();
        clear.Content(winrt::box_value(winrt::hstring(L"Clear history")));
        clear.Click([](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            SharedClipStore().Clear(/*includePinned=*/false);
        });
        body.Children().Append(SettingRow(L"Clear history", L"Remove all unpinned clips.", clear));
    }

    // ---- About ----
    body.Children().Append(SectionHeader(L"About"));
    {
        auto check = winrt::Button();
        check.Content(winrt::box_value(winrt::hstring(L"Check for updates")));
        check.Click([](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            Updater::CheckNow();
        });
        body.Children().Append(SettingRow(L"SuperWin v" SUPERWIN_VERSION_WSTRING,
                                          L"Your Windows multi-tool.", check));
    }

    return std::make_unique<SimplePage>(ui::Page(L"Settings", body));
}

}  // namespace superwin
