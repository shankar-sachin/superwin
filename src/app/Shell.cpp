#include "app/Shell.h"

#include <Windows.h>

#include <filesystem>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Windowing.h>

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

    nav_ = winrt::NavigationView();
    nav_.PaneDisplayMode(winrt::NavigationViewPaneDisplayMode::Left);
    nav_.IsSettingsVisible(false);
    nav_.IsBackButtonVisible(winrt::NavigationViewBackButtonVisible::Collapsed);
    nav_.PaneTitle(L"SuperWin");
    nav_.IsPaneToggleButtonVisible(true);

    // Home, then the four modules. Glyphs are Segoe Fluent Icons codepoints.
    nav_.MenuItems().Append(MakeNavItem(L"Home", 0xE80F, L"home"));

    auto modulesHeader = winrt::NavigationViewItemHeader();
    modulesHeader.Content(winrt::box_value(winrt::hstring(L"Tools")));
    nav_.MenuItems().Append(modulesHeader);

    nav_.MenuItems().Append(MakeNavItem(L"Volume Customizer", 0xE767, L"volume"));
    nav_.MenuItems().Append(MakeNavItem(L"Clipboard++",       0xE8C8, L"clipboard"));
    nav_.MenuItems().Append(MakeNavItem(L"Diagnostics",       0xE9D9, L"diagnostics"));
    nav_.MenuItems().Append(MakeNavItem(L"Notepad Super",     0xE70F, L"notepad"));
    nav_.MenuItems().Append(MakeNavItem(L"Color Picker",      0xE790, L"colorpicker"));

    nav_.SelectionChanged(
        [this](winrt::NavigationView const&,
               winrt::NavigationViewSelectionChangedEventArgs const& args) {
            if (auto item = args.SelectedItem().try_as<winrt::NavigationViewItem>()) {
                if (auto tag = item.Tag()) {
                    OnSelectionChanged(winrt::unbox_value<winrt::hstring>(tag));
                }
            }
        });

    window_.Content(nav_);

    // Start on Home.
    nav_.SelectedItem(nav_.MenuItems().GetAt(0));
    return window_;
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
