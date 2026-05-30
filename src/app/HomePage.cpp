#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "Version.h"
#include "app/Ui.h"

namespace winrt {
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

winrt::FontIcon GlyphIcon(wchar_t cp, double size = 24) {
    winrt::FontIcon icon;
    icon.Glyph(winrt::hstring(&cp, 1));
    icon.FontSize(size);
    return icon;
}

// A clickable PowerToys "quick access" tile: icon over a label, navigates on click.
winrt::Button Tile(winrt::hstring name, wchar_t glyph, winrt::hstring tag,
                   std::function<void(winrt::hstring)> const& navigate) {
    auto col = ui::VStack(8);
    col.HorizontalAlignment(winrt::HorizontalAlignment::Center);
    col.VerticalAlignment(winrt::VerticalAlignment::Center);
    col.Children().Append(GlyphIcon(glyph, 26));
    auto label = ui::Text(name, 13, false);
    label.HorizontalAlignment(winrt::HorizontalAlignment::Center);
    label.TextAlignment(winrt::TextAlignment::Center);
    col.Children().Append(label);

    winrt::Button b;
    b.Content(col);
    b.Width(150);
    b.Height(104);
    b.CornerRadius(winrt::CornerRadius{8, 8, 8, 8});
    b.Click([navigate, tag](winrt::Windows::Foundation::IInspectable const&,
                            winrt::RoutedEventArgs const&) {
        if (navigate) navigate(tag);
    });
    return b;
}

}  // namespace

std::unique_ptr<IModulePage> MakeHomePage(std::function<void(winrt::hstring)> navigate) {
    auto body = ui::VStack(20);

    // Hero card.
    {
        auto inner = ui::VStack(4);
        inner.Children().Append(ui::Text(L"Welcome to SuperWin", 18, true));
        inner.Children().Append(ui::Caption(
            L"Your Windows multi-tool  \x2022  v" SUPERWIN_VERSION_WSTRING));
        body.Children().Append(ui::Card(inner));
    }

    // Quick access tiles.
    {
        auto section = ui::VStack(10);
        section.Children().Append(ui::Text(L"Quick access", 15, true));
        auto row = ui::HStack(12);
        row.Children().Append(Tile(L"Volume\nCustomizer", 0xE767, L"volume", navigate));
        row.Children().Append(Tile(L"Clipboard++", 0xE8C8, L"clipboard", navigate));
        row.Children().Append(Tile(L"Diagnostics", 0xE9D9, L"diagnostics", navigate));
        row.Children().Append(Tile(L"Notepad\nSuper", 0xE70F, L"notepad", navigate));
        row.Children().Append(Tile(L"Color\nPicker", 0xE790, L"colorpicker", navigate));
        section.Children().Append(ui::Card(row));
        body.Children().Append(section);
    }

    return std::make_unique<SimplePage>(ui::Page(L"Home", body));
}

}  // namespace superwin
