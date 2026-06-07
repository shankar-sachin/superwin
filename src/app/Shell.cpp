#include "app/Shell.h"

#include <Windows.h>

#include <filesystem>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Windowing.h>

#include "app/AppHost.h"
#include "core/Settings.h"
#include "core/Strings.h"

namespace winrt {
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
}  // namespace winrt

namespace superwin {
namespace {

// Build a one-character glyph string from a Segoe Fluent Icons codepoint, so no
// non-ASCII bytes live in this source file.
winrt::hstring Glyph(wchar_t codepoint) {
    return winrt::hstring(&codepoint, 1);
}

winrt::NavigationViewItem MakeNavItem(winrt::hstring text, wchar_t glyph,
                                      winrt::hstring tag) {
    winrt::NavigationViewItem item;
    item.Content(winrt::box_value(text));
    winrt::FontIcon icon;
    icon.Glyph(Glyph(glyph));
    item.Icon(icon);
    item.Tag(winrt::box_value(tag));
    return item;
}

}  // namespace

// Slim, transparent title row that the Mica surface shows through. Carries the
// app icon + title on the left; the right ~140px is left clear for the system
// caption buttons (min / restore / close), which are drawn transparently on top.
winrt::Grid Shell::BuildTitleBar() {
    auto bar = winrt::Grid();
    bar.Height(40);
    bar.Background(nullptr);  // let Mica through

    auto content = winrt::Microsoft::UI::Xaml::Controls::StackPanel();
    content.Orientation(winrt::Orientation::Horizontal);
    content.Spacing(8);
    content.VerticalAlignment(winrt::VerticalAlignment::Center);
    content.Margin(winrt::Thickness{14, 0, 0, 0});

    wchar_t exePath[MAX_PATH];
    ::GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    auto ico = std::filesystem::path(exePath).parent_path() / L"SuperWin.ico";
    if (std::filesystem::exists(ico)) {
        winrt::Microsoft::UI::Xaml::Controls::Image img;
        winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage bmp;
        bmp.UriSource(winrt::Windows::Foundation::Uri(winrt::hstring(ico.wstring())));
        img.Source(bmp);
        img.Width(16);
        img.Height(16);
        content.Children().Append(img);
    }

    auto title = winrt::Microsoft::UI::Xaml::Controls::TextBlock();
    title.Text(L"SuperWin");
    title.FontSize(12.5);
    title.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    title.VerticalAlignment(winrt::VerticalAlignment::Center);
    content.Children().Append(title);

    bar.Children().Append(content);
    return bar;
}

winrt::Microsoft::UI::Xaml::Window Shell::Create() {
    window_ = winrt::Window();
    window_.Title(L"SuperWin");
    window_.SystemBackdrop(winrt::MicaBackdrop());

    // Window/taskbar icon, loaded from the .ico deployed next to the exe.
    wchar_t exePath[MAX_PATH];
    ::GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    auto ico = std::filesystem::path(exePath).parent_path() / L"SuperWin.ico";
    if (std::filesystem::exists(ico)) {
        window_.AppWindow().SetIcon(winrt::hstring(ico.wstring()));
    }
    window_.AppWindow().Resize({1120, 760});

    // Open maximized (fills the work area, keeps the title bar/caption buttons).
    // The 1120x760 above is the size the window restores to when un-maximized.
    if (auto p = window_.AppWindow().Presenter()
                     .try_as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>()) {
        p.Maximize();
    }

    // Blend the title bar into the app: extend content underneath, draw the
    // caption buttons transparently over Mica.
    window_.ExtendsContentIntoTitleBar(true);
    if (auto tb = window_.AppWindow().TitleBar()) {
        const winrt::Windows::UI::Color clear{0, 0, 0, 0};
        tb.ButtonBackgroundColor(clear);
        tb.ButtonInactiveBackgroundColor(clear);
    }

    nav_ = winrt::NavigationView();
    nav_.PaneDisplayMode(winrt::NavigationViewPaneDisplayMode::Left);
    nav_.IsSettingsVisible(true);
    nav_.IsBackButtonVisible(winrt::NavigationViewBackButtonVisible::Collapsed);
    nav_.PaneTitle(L"SuperWin");
    nav_.IsPaneToggleButtonVisible(true);

    // Home, then the modules grouped into categories. Glyphs are Segoe Fluent
    // Icons codepoints.
    nav_.MenuItems().Append(MakeNavItem(L"Home", 0xE80F, L"home"));

    // Each category gets a text header AND a separator line in front of it. The
    // header text collapses to nothing when the pane is minimized, but the
    // separator stays visible, so categories are still divided in compact mode.
    auto firstHeader = std::make_shared<bool>(true);
    auto header = [this, firstHeader](const wchar_t* title) {
        if (!*firstHeader) nav_.MenuItems().Append(winrt::NavigationViewItemSeparator());
        *firstHeader = false;
        auto h = winrt::NavigationViewItemHeader();
        h.Content(winrt::box_value(winrt::hstring(title)));
        nav_.MenuItems().Append(h);
    };
    auto item = [this](const wchar_t* name, wchar_t glyph, const wchar_t* tag) {
        nav_.MenuItems().Append(MakeNavItem(name, glyph, tag));
    };

    header(L"System");
    item(L"Volume Customizer", 0xE767, L"volume");
    item(L"Diagnostics",       0xE9D9, L"diagnostics");
    item(L"Network Info",      0xE968, L"netinfo");
    item(L"Sleep",             0xE708, L"keepawake");
    item(L"Always On Top",     0xE840, L"alwaystop");

    header(L"Clipboard & Notepad");
    item(L"Clipboard",     0xE8C8, L"clipboard");
    item(L"Notepad Super", 0xE70F, L"notepad");
    item(L"Text Tools",    0xE8D2, L"text");

    header(L"Developer");
    item(L"Hash & Checksum", 0xE72E, L"hash");
    item(L"JSON Formatter",  0xE943, L"json");
    item(L"GUID Generator",  0xE928, L"guid");
    item(L"Python IDE",      0xE943, L"python");

    header(L"Math");
    item(L"Unit Converter", 0xE8EF, L"convert");
    item(L"Calculator",     0xE9D2, L"calc");

    header(L"Security & Privacy");
    item(L"Security & Privacy", 0xEA18, L"security");
    item(L"Password Generator", 0xE8D7, L"password");

    header(L"Media");
    item(L"Color Picker",   0xE790, L"colorpicker");
    item(L"File Converter", 0xE8B5, L"fileconv");
    item(L"Snake",          0xE7FC, L"snake");

    nav_.SelectionChanged(
        [this](winrt::NavigationView const&,
               winrt::NavigationViewSelectionChangedEventArgs const& args) {
            if (args.IsSettingsSelected()) {
                OnSelectionChanged(L"settings");
                return;
            }
            if (auto item = args.SelectedItem().try_as<winrt::NavigationViewItem>()) {
                if (auto tag = item.Tag()) {
                    OnSelectionChanged(winrt::unbox_value<winrt::hstring>(tag));
                }
            }
        });

    // Two-row root: blended title bar on top, navigation below.
    auto titleBar = BuildTitleBar();
    window_.SetTitleBar(titleBar);

    root_ = winrt::Grid();
    auto r0 = winrt::RowDefinition(); r0.Height(winrt::GridLengthHelper::Auto());
    auto r1 = winrt::RowDefinition(); r1.Height(winrt::GridLengthHelper::FromValueAndType(1, winrt::GridUnitType::Star));
    root_.RowDefinitions().Append(r0);
    root_.RowDefinitions().Append(r1);
    winrt::Grid::SetRow(titleBar, 0);
    winrt::Grid::SetRow(nav_, 1);
    root_.Children().Append(titleBar);
    root_.Children().Append(nav_);
    window_.Content(root_);

    // Apply the saved theme + always-on-top preference, then start on Home.
    SetTheme(winrt::hstring(Utf8ToWide(
        Settings::Instance().GetString("ui.theme", "system"))));
    SetAlwaysOnTop(Settings::Instance().GetBool("ui.alwaysOnTop", false));
    nav_.SelectedItem(nav_.MenuItems().GetAt(0));
    return window_;
}

void Shell::SetTheme(winrt::hstring mode) {
    winrt::ElementTheme theme = winrt::ElementTheme::Default;
    if (mode == L"light") theme = winrt::ElementTheme::Light;
    else if (mode == L"dark") theme = winrt::ElementTheme::Dark;
    if (root_) root_.RequestedTheme(theme);
}

void Shell::SetAlwaysOnTop(bool on) {
    if (!window_) return;
    if (auto p = window_.AppWindow().Presenter()
                     .try_as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>()) {
        p.IsAlwaysOnTop(on);
    }
}

IModulePage* Shell::EnsurePage(winrt::hstring const& tag) {
    if (auto it = pages_.find(tag); it != pages_.end()) return it->second.get();

    std::unique_ptr<IModulePage> page;
    if (tag == L"home") {
        page = MakeHomePage([this](winrt::hstring t) { Navigate(t); });
    } else if (tag == L"volume") {
        page = MakeVolumePage();
    } else if (tag == L"clipboard") {
        page = MakeClipboardPage();
    } else if (tag == L"diagnostics") {
        page = MakeDiagnosticsPage();
    } else if (tag == L"notepad") {
        page = MakeNotepadPage();
    } else if (tag == L"colorpicker") {
        page = MakeColorPickerPage();
    } else if (tag == L"keepawake") {
        page = MakeKeepAwakePage();
    } else if (tag == L"hash") {
        page = MakeHashPage();
    } else if (tag == L"netinfo") {
        page = MakeNetInfoPage();
    } else if (tag == L"convert") {
        page = MakeConvertPage();
    } else if (tag == L"password") {
        page = MakePasswordPage();
    } else if (tag == L"text") {
        page = MakeTextPage();
    } else if (tag == L"json") {
        page = MakeJsonPage();
    } else if (tag == L"guid") {
        page = MakeGuidPage();
    } else if (tag == L"graph") {
        page = MakeGraphPage();
    } else if (tag == L"calc") {
        page = MakeCalcPage();
    } else if (tag == L"python") {
        page = MakePythonPage();
    } else if (tag == L"fileconv") {
        page = MakeFileConvertPage();
    } else if (tag == L"snake") {
        page = MakeSnakePage();
    } else if (tag == L"security") {
        page = MakeSecurityPage();
    } else if (tag == L"alwaystop") {
        page = MakeAlwaysOnTopPage(host_);
    } else if (tag == L"settings") {
        page = MakeSettingsPage(this, host_);
    }
    if (!page) return nullptr;
    IModulePage* raw = page.get();
    pages_.emplace(tag, std::move(page));
    return raw;
}

void Shell::OnSelectionChanged(winrt::hstring tag) {
    IModulePage* page = EnsurePage(tag);
    if (!page) return;
    if (current_ && current_ != page) current_->OnHidden();
    current_ = page;
    nav_.Content(page->Root());
    page->OnShown();
}

void Shell::Navigate(winrt::hstring tag) {
    for (auto const& obj : nav_.MenuItems()) {
        if (auto item = obj.try_as<winrt::NavigationViewItem>()) {
            if (auto t = item.Tag(); t && winrt::unbox_value<winrt::hstring>(t) == tag) {
                nav_.SelectedItem(item);
                return;
            }
        }
    }
}

}  // namespace superwin
