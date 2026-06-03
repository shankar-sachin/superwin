// Graphing Calculator page: type f(x), pick an x-range, and plot. The expression
// is compiled by GraphLogic (superwin_core, unit-tested); this file samples it and
// renders the curve + axes onto a Canvas using WinUI shapes.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>

#include "app/Ui.h"
#include "core/Strings.h"
#include "modules/graph/GraphLogic.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Shapes;
}  // namespace winrt

namespace superwin {
namespace {

constexpr winrt::Windows::UI::Color kCurve{255, 0x5b, 0x8c, 0xff};

class GraphPage : public IModulePage {
public:
    GraphPage() { Build(); }
    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }

private:
    void Build() {
        editor_ = winrt::TextBox();
        editor_.PlaceholderText(L"f(x) =  e.g.  sin(x)*x");
        editor_.Text(L"sin(x)");
        editor_.FontFamily(winrt::FontFamily(L"Consolas"));
        editor_.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);

        xminBox_ = MakeNum(-10);
        xmaxBox_ = MakeNum(10);

        auto plot = winrt::Button();
        plot.Content(winrt::box_value(winrt::hstring(L"Plot")));
        plot.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { Recompile(); Render(); });

        auto topRow = ui::HStack(8);
        topRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        auto fx = ui::Text(L"f(x) =", 14, true);
        fx.VerticalAlignment(winrt::VerticalAlignment::Center);
        topRow.Children().Append(fx);
        editor_.MinWidth(280);
        topRow.Children().Append(editor_);
        topRow.Children().Append(plot);

        auto rangeRow = ui::HStack(10);
        rangeRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        rangeRow.Children().Append(ui::Text(L"x from", 13));
        rangeRow.Children().Append(xminBox_);
        rangeRow.Children().Append(ui::Text(L"to", 13));
        rangeRow.Children().Append(xmaxBox_);

        status_ = ui::Caption(L"");

        canvas_ = winrt::Canvas();
        canvas_.Height(360);
        canvas_.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);
        if (auto bg = ui::ThemeBrush(L"CardBackgroundFillColorSecondaryBrush")) canvas_.Background(bg);
        canvas_.SizeChanged([this](winrt::IInspectable const&, winrt::SizeChangedEventArgs const&) { Render(); });

        auto card = ui::VStack(12);
        card.Children().Append(topRow);
        card.Children().Append(rangeRow);
        card.Children().Append(status_);
        card.Children().Append(ui::Card(canvas_, 6));

        root_ = ui::Page(L"Graphing Calculator", ui::Card(card));
        Recompile();
    }

    winrt::NumberBox MakeNum(double v) {
        winrt::NumberBox n;
        n.Value(v);
        n.SmallChange(1);
        n.SpinButtonPlacementMode(winrt::NumberBoxSpinButtonPlacementMode::Inline);
        n.Width(130);
        n.ValueChanged([this](winrt::NumberBox const&, winrt::NumberBoxValueChangedEventArgs const&) { Render(); });
        return n;
    }

    void Recompile() {
        std::string err;
        auto fn = CompileExpression(WideToUtf8(std::wstring(editor_.Text())), err);
        if (fn) {
            fn_ = *fn;
            haveFn_ = true;
            status_.Text(L"");
        } else {
            haveFn_ = false;
            status_.Text(winrt::hstring(L"\x26A0  " + Utf8ToWide(err)));
        }
    }

    winrt::Line Axis(double x1, double y1, double x2, double y2) {
        winrt::Line l;
        l.X1(x1); l.Y1(y1); l.X2(x2); l.Y2(y2);
        if (auto b = ui::ThemeBrush(L"TextFillColorTertiaryBrush")) l.Stroke(b);
        l.StrokeThickness(1);
        return l;
    }

    void Render() {
        if (!canvas_) return;
        canvas_.Children().Clear();
        const double W = canvas_.ActualWidth();
        const double H = canvas_.ActualHeight();
        if (W < 2 || H < 2 || !haveFn_) return;

        double xmin = xminBox_.Value();
        double xmax = xmaxBox_.Value();
        if (!(xmax > xmin)) { status_.Text(L"x range must have max > min"); return; }

        // Sample.
        const int N = 700;
        std::vector<double> ys(N);
        double ymin = 1e300, ymax = -1e300;
        for (int i = 0; i < N; ++i) {
            const double x = xmin + (xmax - xmin) * i / (N - 1);
            const double y = fn_(x);
            ys[i] = y;
            if (std::isfinite(y)) { ymin = (std::min)(ymin, y); ymax = (std::max)(ymax, y); }
        }
        if (ymin > ymax) { ymin = -1; ymax = 1; }
        if (ymax - ymin < 1e-9) { ymin -= 1; ymax += 1; }
        const double pad = (ymax - ymin) * 0.08;
        ymin -= pad; ymax += pad;

        auto sx = [&](double x) { return (x - xmin) / (xmax - xmin) * W; };
        auto sy = [&](double y) { return H - (y - ymin) / (ymax - ymin) * H; };

        // Axes (drawn only when 0 falls inside the range).
        if (xmin < 0 && xmax > 0) canvas_.Children().Append(Axis(sx(0), 0, sx(0), H));
        if (ymin < 0 && ymax > 0) canvas_.Children().Append(Axis(0, sy(0), W, sy(0)));

        // Curve, split into segments at non-finite values or asymptote jumps.
        winrt::SolidColorBrush curve{kCurve};
        winrt::Polyline poly;
        poly.Stroke(curve);
        poly.StrokeThickness(2);
        double prevPy = 0; bool have = false;
        auto flush = [&] {
            if (poly.Points().Size() >= 2) canvas_.Children().Append(poly);
            poly = winrt::Polyline();
            poly.Stroke(curve);
            poly.StrokeThickness(2);
        };
        for (int i = 0; i < N; ++i) {
            const double y = ys[i];
            if (!std::isfinite(y)) { flush(); have = false; continue; }
            const double px = sx(xmin + (xmax - xmin) * i / (N - 1));
            const double py = sy(y);
            if (have && std::fabs(py - prevPy) > H) { flush(); }  // asymptote
            poly.Points().Append(winrt::Point{static_cast<float>(px), static_cast<float>(py)});
            prevPy = py; have = true;
        }
        flush();

        wchar_t buf[96];
        swprintf_s(buf, L"y \x2208 [%.3g, %.3g]", ymin + pad, ymax - pad);
        status_.Text(buf);
    }

    winrt::Microsoft::UI::Xaml::Controls::TextBox editor_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::NumberBox xminBox_{nullptr}, xmaxBox_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock status_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Canvas canvas_{nullptr};
    GraphFn fn_;
    bool haveFn_ = false;
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeGraphPage() {
    return std::make_unique<GraphPage>();
}

}  // namespace superwin
