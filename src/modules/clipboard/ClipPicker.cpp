#include "modules/clipboard/ClipPicker.h"

#include <algorithm>
#include <chrono>
#include <string>

#include <microsoft.ui.xaml.window.h>  // IWindowNative (get the picker's HWND)

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>

#include "app/Ui.h"
#include "core/Settings.h"
#include "core/Strings.h"
#include "modules/clipboard/ClipStore.h"
#include "modules/clipboard/ClipText.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Input;
using namespace winrt::Microsoft::UI::Xaml::Media;
}  // namespace winrt

namespace superwin {
namespace {

constexpr int kWidth = 870;
constexpr int kHeight = 1290;

winrt::hstring Preview(const std::string& utf8) {
    std::wstring w = Utf8ToWide(utf8);
    for (auto& c : w) if (c == L'\r' || c == L'\n' || c == L'\t') c = L' ';
    if (w.size() > 200) w = w.substr(0, 200) + L"\x2026";
    return winrt::hstring(w);
}

winrt::ElementTheme ThemeFromSettings() {
    const std::string t = Settings::Instance().GetString("ui.theme", "system");
    if (t == "light") return winrt::ElementTheme::Light;
    if (t == "dark")  return winrt::ElementTheme::Dark;
    return winrt::ElementTheme::Default;
}

// Reliably bring a window to the foreground even when our process is in the
// background (a global hotkey spawned us). Windows blocks a background process
// from stealing focus, so we briefly attach to the current foreground thread's
// input queue, which lifts that restriction for the duration of the call.
void ForceForeground(HWND hwnd) {
    if (!hwnd) return;
    const HWND fg = ::GetForegroundWindow();
    const DWORD fgThread = ::GetWindowThreadProcessId(fg, nullptr);
    const DWORD myThread = ::GetCurrentThreadId();
    const bool attached = fgThread && fgThread != myThread &&
                          ::AttachThreadInput(myThread, fgThread, TRUE);
    ::SetForegroundWindow(hwnd);
    ::SetFocus(hwnd);
    ::BringWindowToTop(hwnd);
    if (attached) ::AttachThreadInput(myThread, fgThread, FALSE);
}

}  // namespace

ClipPicker::ClipPicker() {
    autoPaste_ = Settings::Instance().GetBool("clipboard.autoPaste", true);
}

void ClipPicker::Build() {
    if (built_) return;
    built_ = true;

    window_ = winrt::Window();
    window_.Title(L"Clipboard");
    window_.SystemBackdrop(winrt::DesktopAcrylicBackdrop());

    appWindow_ = window_.AppWindow();
    if (auto p = winrt::Microsoft::UI::Windowing::OverlappedPresenter::Create()) {
        p.IsAlwaysOnTop(true);
        p.IsResizable(false);
        p.IsMaximizable(false);
        p.IsMinimizable(false);
        p.SetBorderAndTitleBar(true, false);
        appWindow_.SetPresenter(p);
    }

    search_ = winrt::TextBox();
    search_.PlaceholderText(L"Search clipboard\x2026");
    search_.FontSize(14);
    search_.CornerRadius(winrt::CornerRadius{8, 8, 8, 8});
    search_.TextChanged([this](winrt::IInspectable const&, winrt::IInspectable const&) { Populate(); });
    search_.KeyDown([this](winrt::IInspectable const&, winrt::KeyRoutedEventArgs const& e) {
        if (e.Key() == winrt::Windows::System::VirtualKey::Enter) {
            ChooseSelected();
            e.Handled(true);
        } else if (e.Key() == winrt::Windows::System::VirtualKey::Escape) {
            Hide();
            e.Handled(true);
        } else if (e.Key() == winrt::Windows::System::VirtualKey::Down) {
            if (list_.Items().Size() > 0) {
                list_.SelectedIndex(0);
                if (auto c = list_.ContainerFromIndex(0).try_as<winrt::Control>()) c.Focus(winrt::FocusState::Keyboard);
            }
            e.Handled(true);
        }
    });

    list_ = winrt::ListView();
    list_.SelectionMode(winrt::ListViewSelectionMode::Single);
    list_.KeyDown([this](winrt::IInspectable const&, winrt::KeyRoutedEventArgs const& e) {
        if (e.Key() == winrt::Windows::System::VirtualKey::Enter) {
            ChooseSelected();
            e.Handled(true);
        } else if (e.Key() == winrt::Windows::System::VirtualKey::Escape) {
            Hide();
            e.Handled(true);
        }
    });

    // Header: a title + a subtle hint, like the Win+V flyout but tidier.
    auto title = ui::Text(L"Clipboard", 18, true);
    auto header = ui::VStack(2);
    header.Children().Append(title);
    header.Children().Append(ui::Caption(L"Pick a clip to paste it back where you were."));

    // Footer: keyboard hints.
    auto footer = ui::Caption(L"\x2191\x2193 navigate   \x2022   Enter paste   \x2022   Esc close");
    footer.FontSize(11.5);
    footer.HorizontalAlignment(winrt::HorizontalAlignment::Center);

    list_.CornerRadius(winrt::CornerRadius{8, 8, 8, 8});

    auto root = winrt::Grid();
    root.Padding(winrt::Thickness{16, 14, 16, 12});
    root.RowSpacing(10);
    root.RequestedTheme(ThemeFromSettings());
    auto r0 = winrt::RowDefinition(); r0.Height(winrt::GridLengthHelper::Auto());
    auto r1 = winrt::RowDefinition(); r1.Height(winrt::GridLengthHelper::Auto());
    auto r2 = winrt::RowDefinition(); r2.Height(winrt::GridLengthHelper::FromValueAndType(1, winrt::GridUnitType::Star));
    auto r3 = winrt::RowDefinition(); r3.Height(winrt::GridLengthHelper::Auto());
    root.RowDefinitions().Append(r0);
    root.RowDefinitions().Append(r1);
    root.RowDefinitions().Append(r2);
    root.RowDefinitions().Append(r3);
    winrt::Grid::SetRow(header, 0);
    winrt::Grid::SetRow(search_, 1);
    winrt::Grid::SetRow(list_, 2);
    winrt::Grid::SetRow(footer, 3);
    root.Children().Append(header);
    root.Children().Append(search_);
    root.Children().Append(list_);
    root.Children().Append(footer);
    window_.Content(root);

    // Auto-dismiss the moment focus leaves the picker. The deactivation that
    // happens *during* the show/foreground hand-off is suppressed, otherwise the
    // picker would hide itself the instant it appears; once it genuinely
    // activates we arm the dismiss-on-deactivate behaviour.
    window_.Activated([this](winrt::IInspectable const&, winrt::WindowActivatedEventArgs const& e) {
        if (e.WindowActivationState() == winrt::WindowActivationState::Deactivated) {
            if (!suppressDeactivate_) Hide();
        } else {
            suppressDeactivate_ = false;  // a real activation -- safe to arm
        }
    });

    auto native = window_.try_as<::IWindowNative>();
    if (native) native->get_WindowHandle(&hwnd_);
}

void ClipPicker::Populate() {
    list_.Items().Clear();
    const std::string filter = WideToUtf8(std::wstring(search_.Text()));

    auto items = SharedClipStore().Items(filter);
    // Pinned first, otherwise newest-first (Items() is already newest-first).
    std::stable_sort(items.begin(), items.end(),
        [](const ClipItem& a, const ClipItem& b) { return a.pinned && !b.pinned; });

    if (items.empty()) {
        winrt::ListViewItem empty;
        empty.Content(winrt::box_value(winrt::hstring(L"No clips yet \x2014 copy some text.")));
        empty.IsHitTestVisible(false);
        list_.Items().Append(empty);
        return;
    }

    for (const auto& item : items) {
        const uint64_t id = item.id;

        // [pin?] [preview text *] [✕ delete]
        auto grid = winrt::Grid();
        grid.VerticalAlignment(winrt::VerticalAlignment::Center);
        auto c0 = winrt::ColumnDefinition(); c0.Width(winrt::GridLengthHelper::Auto());
        auto c1 = winrt::ColumnDefinition(); c1.Width(winrt::GridLengthHelper::FromValueAndType(1, winrt::GridUnitType::Star));
        auto c2 = winrt::ColumnDefinition(); c2.Width(winrt::GridLengthHelper::Auto());
        grid.ColumnDefinitions().Append(c0);
        grid.ColumnDefinitions().Append(c1);
        grid.ColumnDefinitions().Append(c2);
        grid.ColumnSpacing(10);

        if (item.pinned) {
            winrt::FontIcon pin;
            wchar_t glyph = 0xE840;  // pinned glyph
            pin.Glyph(winrt::hstring(&glyph, 1));
            pin.FontSize(13);
            pin.VerticalAlignment(winrt::VerticalAlignment::Center);
            if (auto accent = ui::ThemeBrush(L"AccentTextFillColorPrimaryBrush")) pin.Foreground(accent);
            winrt::Grid::SetColumn(pin, 0);
            grid.Children().Append(pin);
        }

        auto text = ui::Text(Preview(item.text), 14);
        text.TextTrimming(winrt::TextTrimming::CharacterEllipsis);
        text.TextWrapping(winrt::TextWrapping::NoWrap);
        text.VerticalAlignment(winrt::VerticalAlignment::Center);
        winrt::Grid::SetColumn(text, 1);
        grid.Children().Append(text);

        // Delete affordance: a rounded ✕ that removes just this clip. It marks the
        // tap handled so the row's paste-on-tap doesn't also fire.
        auto del = winrt::Border();
        del.CornerRadius(winrt::CornerRadius{6, 6, 6, 6});
        del.Padding(winrt::Thickness{7, 3, 7, 3});
        del.VerticalAlignment(winrt::VerticalAlignment::Center);
        auto x = ui::Text(L"\x2715", 13);
        if (auto sec = ui::ThemeBrush(L"TextFillColorSecondaryBrush")) x.Foreground(sec);
        del.Child(x);
        winrt::Controls::ToolTipService::SetToolTip(del, winrt::box_value(winrt::hstring(L"Delete this clip")));
        del.Tapped([this, id](winrt::IInspectable const&, winrt::Input::TappedRoutedEventArgs const& e) {
            e.Handled(true);
            SharedClipStore().Remove(id);
            Populate();
        });
        winrt::Grid::SetColumn(del, 2);
        grid.Children().Append(del);

        // The bordered "card" around each clip.
        auto card = winrt::Border();
        card.Child(grid);
        card.CornerRadius(winrt::CornerRadius{10, 10, 10, 10});
        card.Padding(winrt::Thickness{14, 10, 8, 10});
        card.Margin(winrt::Thickness{0, 0, 0, 8});
        if (auto bg = ui::ThemeBrush(L"CardBackgroundFillColorDefaultBrush")) card.Background(bg);
        if (auto st = ui::ThemeBrush(L"CardStrokeColorDefaultBrush")) {
            card.BorderBrush(st);
            card.BorderThickness(winrt::Thickness{1, 1, 1, 1});
        }
        card.Tapped([this, id](winrt::IInspectable const&, winrt::Input::TappedRoutedEventArgs const& e) {
            e.Handled(true);
            ChooseId(id);
        });

        winrt::ListViewItem lvi;
        lvi.Content(card);
        lvi.Tag(winrt::box_value(item.id));
        lvi.Padding(winrt::Thickness{0, 0, 0, 0});
        lvi.HorizontalContentAlignment(winrt::HorizontalAlignment::Stretch);
        lvi.Background(nullptr);
        list_.Items().Append(lvi);
    }
    list_.SelectedIndex(0);
}

void ClipPicker::Show(HWND previousForeground) {
    previous_ = previousForeground;
    autoPaste_ = Settings::Instance().GetBool("clipboard.autoPaste", true);
    Build();

    search_.Text(L"");
    Populate();

    // Position at the cursor, clamped to the monitor work area.
    POINT pt{};
    ::GetCursorPos(&pt);
    int x = pt.x, y = pt.y;
    if (HMONITOR mon = ::MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST)) {
        MONITORINFO mi{sizeof(mi)};
        if (::GetMonitorInfoW(mon, &mi)) {
            x = (std::min)(x, static_cast<int>(mi.rcWork.right) - kWidth);
            y = (std::min)(y, static_cast<int>(mi.rcWork.bottom) - kHeight);
            x = (std::max)(x, static_cast<int>(mi.rcWork.left));
            y = (std::max)(y, static_cast<int>(mi.rcWork.top));
        }
    }
    appWindow_.MoveAndResize({x, y, kWidth, kHeight});

    // Ignore the transient deactivation while we grab the foreground.
    suppressDeactivate_ = true;
    window_.Activate();
    ForceForeground(hwnd_);
    search_.Focus(winrt::FocusState::Programmatic);
}

void ClipPicker::Hide() {
    if (appWindow_) appWindow_.Hide();
}

void ClipPicker::ChooseSelected() {
    int index = list_.SelectedIndex();
    if (index < 0 && list_.Items().Size() > 0) index = 0;
    if (index < 0) return;
    if (auto item = list_.Items().GetAt(index).try_as<winrt::ListViewItem>()) {
        if (auto tag = item.Tag()) ChooseId(winrt::unbox_value<uint64_t>(tag));
    }
}

void ClipPicker::ChooseId(uint64_t id) {
    auto clip = SharedClipStore().Get(id);
    if (!clip) return;
    WriteClipboardText(Utf8ToWide(clip->text));
    Hide();
    PasteIntoPrevious();
}

void ClipPicker::PasteIntoPrevious() {
    if (!previous_) return;
    ::SetForegroundWindow(previous_);
    if (!autoPaste_) return;

    // Let the target window regain focus before synthesizing Ctrl+V; doing it
    // immediately tends to drop the keystroke.
    if (!pasteTimer_) {
        pasteTimer_ = winrt::DispatcherTimer();
        pasteTimer_.Interval(std::chrono::milliseconds(120));
        pasteTimer_.Tick([this](winrt::IInspectable const&, winrt::IInspectable const&) {
            pasteTimer_.Stop();
            // The trigger chord (e.g. Win+Shift+V) may still be physically held;
            // if so, our synthetic Ctrl+V becomes Ctrl+Shift+V / Win+Ctrl+V and is
            // silently dropped. Release any still-down modifiers first.
            const BYTE mods[] = {VK_LWIN, VK_RWIN, VK_SHIFT, VK_LSHIFT, VK_RSHIFT,
                                 VK_MENU,  VK_LMENU, VK_RMENU,
                                 VK_CONTROL, VK_LCONTROL, VK_RCONTROL};
            std::vector<INPUT> ups;
            for (BYTE vk : mods) {
                if (::GetAsyncKeyState(vk) & 0x8000) {
                    INPUT u{}; u.type = INPUT_KEYBOARD; u.ki.wVk = vk; u.ki.dwFlags = KEYEVENTF_KEYUP;
                    ups.push_back(u);
                }
            }
            if (!ups.empty()) ::SendInput(static_cast<UINT>(ups.size()), ups.data(), sizeof(INPUT));

            INPUT in[4]{};
            in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_CONTROL;
            in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = 'V';
            in[2].type = INPUT_KEYBOARD; in[2].ki.wVk = 'V';        in[2].ki.dwFlags = KEYEVENTF_KEYUP;
            in[3].type = INPUT_KEYBOARD; in[3].ki.wVk = VK_CONTROL; in[3].ki.dwFlags = KEYEVENTF_KEYUP;
            ::SendInput(4, in, sizeof(INPUT));
        });
    }
    pasteTimer_.Start();
}

}  // namespace superwin
