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

constexpr int kWidth = 380;
constexpr int kHeight = 460;

winrt::hstring Preview(const std::string& utf8) {
    std::wstring w = Utf8ToWide(utf8);
    for (auto& c : w) if (c == L'\r' || c == L'\n' || c == L'\t') c = L' ';
    if (w.size() > 120) w = w.substr(0, 120) + L"\x2026";
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
    list_.IsItemClickEnabled(true);
    list_.ItemClick([this](winrt::IInspectable const&, winrt::ItemClickEventArgs const& e) {
        if (auto item = e.ClickedItem().try_as<winrt::ListViewItem>()) {
            if (auto tag = item.Tag()) ChooseId(winrt::unbox_value<uint64_t>(tag));
        }
    });
    list_.KeyDown([this](winrt::IInspectable const&, winrt::KeyRoutedEventArgs const& e) {
        if (e.Key() == winrt::Windows::System::VirtualKey::Enter) {
            ChooseSelected();
            e.Handled(true);
        } else if (e.Key() == winrt::Windows::System::VirtualKey::Escape) {
            Hide();
            e.Handled(true);
        }
    });

    auto root = winrt::Grid();
    root.Padding(winrt::Thickness{12, 12, 12, 12});
    root.RowSpacing(10);
    root.RequestedTheme(ThemeFromSettings());
    auto r0 = winrt::RowDefinition(); r0.Height(winrt::GridLengthHelper::Auto());
    auto r1 = winrt::RowDefinition(); r1.Height(winrt::GridLengthHelper::FromValueAndType(1, winrt::GridUnitType::Star));
    root.RowDefinitions().Append(r0);
    root.RowDefinitions().Append(r1);
    winrt::Grid::SetRow(search_, 0);
    winrt::Grid::SetRow(list_, 1);
    root.Children().Append(search_);
    root.Children().Append(list_);
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
        auto row = ui::HStack(8);
        row.VerticalAlignment(winrt::VerticalAlignment::Center);
        if (item.pinned) {
            winrt::FontIcon pin;
            wchar_t glyph = 0xE840;  // pinned glyph
            pin.Glyph(winrt::hstring(&glyph, 1));
            pin.FontSize(12);
            row.Children().Append(pin);
        }
        auto text = ui::Text(Preview(item.text), 13);
        text.TextTrimming(winrt::TextTrimming::CharacterEllipsis);
        text.TextWrapping(winrt::TextWrapping::NoWrap);
        row.Children().Append(text);

        winrt::ListViewItem lvi;
        lvi.Content(row);
        lvi.Tag(winrt::box_value(item.id));
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
        pasteTimer_.Interval(std::chrono::milliseconds(90));
        pasteTimer_.Tick([this](winrt::IInspectable const&, winrt::IInspectable const&) {
            pasteTimer_.Stop();
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
