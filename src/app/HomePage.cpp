#include <memory>
#include <vector>

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

// A clickable PowerToys "quick access" tile: icon over a label, navigates on
// click. The tile stretches horizontally so the responsive grid can fill a row.
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
    b.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);
    b.MinWidth(150);
    b.Height(104);
    b.CornerRadius(winrt::CornerRadius{8, 8, 8, 8});
    b.Click([navigate, tag](winrt::Windows::Foundation::IInspectable const&,
                            winrt::RoutedEventArgs const&) {
        if (navigate) navigate(tag);
    });
    return b;
}

// Lays the tiles out in a grid whose column count adapts to the available width;
// the columns are star-sized, so the tiles snap and fill each row edge-to-edge.
winrt::Grid TileGrid(std::vector<winrt::Button> tiles) {
    constexpr double kTileMin = 150.0;
    constexpr double kGap = 12.0;

    winrt::Grid grid;
    grid.ColumnSpacing(kGap);
    grid.RowSpacing(kGap);

    auto state = std::make_shared<std::vector<winrt::Button>>(std::move(tiles));
    auto lastCols = std::make_shared<int>(0);

    auto relayout = [grid, state, lastCols](double width) {
        const int n = static_cast<int>(state->size());
        if (n == 0) return;
        int cols = static_cast<int>((width + kGap) / (kTileMin + kGap));
        if (cols < 1) cols = 1;
        if (cols > n) cols = n;
        if (cols == *lastCols) return;  // nothing to re-flow
        *lastCols = cols;

        grid.Children().Clear();
        grid.ColumnDefinitions().Clear();
        grid.RowDefinitions().Clear();
        for (int c = 0; c < cols; ++c) {
            winrt::ColumnDefinition cd;
            cd.Width(winrt::GridLengthHelper::FromValueAndType(1, winrt::GridUnitType::Star));
            grid.ColumnDefinitions().Append(cd);
        }
        const int rows = (n + cols - 1) / cols;
        for (int r = 0; r < rows; ++r) {
            winrt::RowDefinition rd;
            rd.Height(winrt::GridLengthHelper::Auto());
            grid.RowDefinitions().Append(rd);
        }
        for (int i = 0; i < n; ++i) {
            auto& t = (*state)[i];
            winrt::Grid::SetColumn(t, i % cols);
            winrt::Grid::SetRow(t, i / cols);
            grid.Children().Append(t);
        }
    };

    grid.SizeChanged([relayout](winrt::Windows::Foundation::IInspectable const&,
                                winrt::SizeChangedEventArgs const& e) {
        relayout(e.NewSize().Width);
    });
    relayout(900);  // initial guess; SizeChanged corrects it once laid out
    return grid;
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

    // Quick access — tools grouped into categories, each a responsive grid.
    {
        auto section = ui::VStack(16);
        section.Children().Append(ui::Text(L"Quick access", 15, true));

        auto group = [&](winrt::hstring title, std::vector<winrt::Button> tiles) {
            auto g = ui::VStack(8);
            g.Children().Append(ui::Caption(title));
            g.Children().Append(ui::Card(TileGrid(std::move(tiles))));
            section.Children().Append(g);
        };

        {
            std::vector<winrt::Button> t;
            t.push_back(Tile(L"Volume\nCustomizer", 0xE767, L"volume", navigate));
            t.push_back(Tile(L"Diagnostics", 0xE9D9, L"diagnostics", navigate));
            t.push_back(Tile(L"Network\nInfo", 0xE968, L"netinfo", navigate));
            t.push_back(Tile(L"Keep\nAwake", 0xE945, L"keepawake", navigate));
            group(L"System", std::move(t));
        }
        {
            std::vector<winrt::Button> t;
            t.push_back(Tile(L"Clipboard", 0xE8C8, L"clipboard", navigate));
            t.push_back(Tile(L"Notepad\nSuper", 0xE70F, L"notepad", navigate));
            t.push_back(Tile(L"Text\nTools", 0xE8D2, L"text", navigate));
            group(L"Clipboard & Notes", std::move(t));
        }
        {
            std::vector<winrt::Button> t;
            t.push_back(Tile(L"Hash &\nChecksum", 0xE72E, L"hash", navigate));
            t.push_back(Tile(L"JSON\nFormatter", 0xE943, L"json", navigate));
            t.push_back(Tile(L"GUID\nGenerator", 0xE928, L"guid", navigate));
            group(L"Developer", std::move(t));
        }
        {
            std::vector<winrt::Button> t;
            t.push_back(Tile(L"Unit\nConverter", 0xE8EF, L"convert", navigate));
            t.push_back(Tile(L"Graphing\nCalculator", 0xE9D2, L"graph", navigate));
            group(L"Math", std::move(t));
        }
        {
            std::vector<winrt::Button> t;
            t.push_back(Tile(L"Password\nGenerator", 0xE8D7, L"password", navigate));
            t.push_back(Tile(L"Security &\nPrivacy", 0xEA18, L"security", navigate));
            group(L"Security & Privacy", std::move(t));
        }
        {
            std::vector<winrt::Button> t;
            t.push_back(Tile(L"Color\nPicker", 0xE790, L"colorpicker", navigate));
            group(L"Media", std::move(t));
        }

        body.Children().Append(section);
    }

    return std::make_unique<SimplePage>(ui::Page(L"Home", body));
}

}  // namespace superwin
