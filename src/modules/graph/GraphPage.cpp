// Graphing Calculator page — a Desmos-style plotter with a built-in CAS:
//   * multiple colour-coded functions, drag to pan, wheel to zoom, labelled grid
//   * a live "pretty math" preview of each function (x², √, ·, …)
//   * per-function d/dx (plot derivative), ∫ dx (plot antiderivative), simplify
//   * a numeric definite-integral panel (Simpson's rule)
// Parsing / CAS live in Expr + GraphLogic (superwin_core, unit-tested); this file
// is the WinUI rendering + interaction.
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>

#include "app/Ui.h"
#include "core/Strings.h"
#include "modules/graph/GraphLogic.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Input;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Shapes;
}  // namespace winrt

namespace superwin {
namespace {

const winrt::Windows::UI::Color kPalette[] = {
    {255, 0x4f, 0x8e, 0xf7}, {255, 0xff, 0x45, 0x4a}, {255, 0x34, 0xc7, 0x59},
    {255, 0xff, 0x9f, 0x0a}, {255, 0xaf, 0x52, 0xde}, {255, 0x5a, 0xc8, 0xfa},
};

double NiceStep(double rough) {
    if (rough <= 0) return 1;
    const double e = std::floor(std::log10(rough));
    const double base = std::pow(10.0, e);
    const double f = rough / base;
    const double nf = f < 1.5 ? 1 : f < 3 ? 2 : f < 7 ? 5 : 10;
    return nf * base;
}

std::wstring FmtNum(double v) { wchar_t b[32]; swprintf_s(b, L"%g", v); return b; }

struct Row {
    winrt::TextBox box{nullptr};
    winrt::TextBlock preview{nullptr};
    winrt::Windows::UI::Color color{};
};

class GraphPage : public IModulePage {
public:
    GraphPage() { Build(); }
    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }

private:
    void Build() {
        exprPanel_ = ui::VStack(10);

        auto add = winrt::Button();
        add.Content(winrt::box_value(winrt::hstring(L"\x2795  Add function")));
        add.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { AddRow(L""); Render(); });

        auto reset = winrt::Button();
        reset.Content(winrt::box_value(winrt::hstring(L"Reset view")));
        reset.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            vxmin_ = -10; vxmax_ = 10; vymin_ = -6; vymax_ = 6; Render();
        });

        auto controls = ui::HStack(8);
        controls.Children().Append(add);
        controls.Children().Append(reset);

        // Definite-integral panel.
        aBox_ = MakeNum(0, false); bBox_ = MakeNum(1, false);
        auto integ = winrt::Button();
        integ.Content(winrt::box_value(winrt::hstring(L"Compute \x222B")));
        integ.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { ComputeArea(); });
        areaLabel_ = ui::Caption(L"");
        auto areaRow = ui::HStack(8);
        areaRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        areaRow.Children().Append(ui::Text(L"\x222B of f\x2081 from", 13));
        areaRow.Children().Append(aBox_);
        areaRow.Children().Append(ui::Text(L"to", 13));
        areaRow.Children().Append(bBox_);
        areaRow.Children().Append(integ);
        areaRow.Children().Append(areaLabel_);

        status_ = ui::Caption(L"Drag to pan  \x2022  scroll to zoom");

        canvas_ = winrt::Canvas();
        canvas_.Height(440);
        canvas_.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);
        if (auto bg = ui::ThemeBrush(L"CardBackgroundFillColorSecondaryBrush")) canvas_.Background(bg);
        canvas_.SizeChanged([this](winrt::IInspectable const&, winrt::SizeChangedEventArgs const&) { Render(); });
        canvas_.PointerPressed([this](winrt::IInspectable const&, winrt::PointerRoutedEventArgs const& e) {
            dragging_ = true;
            auto p = e.GetCurrentPoint(canvas_).Position();
            lastX_ = p.X; lastY_ = p.Y;
            canvas_.CapturePointer(e.Pointer());
        });
        canvas_.PointerMoved([this](winrt::IInspectable const&, winrt::PointerRoutedEventArgs const& e) {
            if (!dragging_) return;
            auto p = e.GetCurrentPoint(canvas_).Position();
            Pan(p.X - lastX_, p.Y - lastY_);
            lastX_ = p.X; lastY_ = p.Y;
        });
        canvas_.PointerReleased([this](winrt::IInspectable const&, winrt::PointerRoutedEventArgs const& e) {
            dragging_ = false; canvas_.ReleasePointerCapture(e.Pointer());
        });
        canvas_.PointerWheelChanged([this](winrt::IInspectable const&, winrt::PointerRoutedEventArgs const& e) {
            auto pt = e.GetCurrentPoint(canvas_);
            const int delta = pt.Properties().MouseWheelDelta();
            Zoom(delta > 0 ? 0.85 : 1.0 / 0.85, pt.Position().X, pt.Position().Y);
        });

        auto card = ui::VStack(12);
        card.Children().Append(exprPanel_);
        card.Children().Append(controls);
        card.Children().Append(ui::Card(areaRow, 10));
        card.Children().Append(status_);
        card.Children().Append(ui::Card(canvas_, 0));

        root_ = ui::Page(L"Graphing Calculator", ui::Card(card));

        AddRow(L"sin(x)");
        Render();
    }

    winrt::NumberBox MakeNum(double v, bool render) {
        winrt::NumberBox n;
        n.Value(v); n.SmallChange(1);
        n.SpinButtonPlacementMode(winrt::NumberBoxSpinButtonPlacementMode::Inline);
        n.Width(110);
        if (render) n.ValueChanged([this](winrt::NumberBox const&, winrt::NumberBoxValueChangedEventArgs const&) { Render(); });
        return n;
    }

    static void SetPreview(winrt::TextBox box, winrt::TextBlock preview) {
        std::string err;
        auto p = PrettyExpr(WideToUtf8(std::wstring(box.Text())), err);
        preview.Text(p ? winrt::hstring(Utf8ToWide(*p)) : winrt::hstring(L""));
    }

    void AddRow(winrt::hstring text) {
        Row r;
        r.color = kPalette[rows_.size() % (sizeof(kPalette) / sizeof(kPalette[0]))];
        winrt::SolidColorBrush colorBrush{r.color};

        winrt::Border swatch;
        swatch.Width(14); swatch.Height(14);
        swatch.CornerRadius(winrt::CornerRadius{4, 4, 4, 4});
        swatch.Background(colorBrush);
        swatch.VerticalAlignment(winrt::VerticalAlignment::Center);

        r.box = winrt::TextBox();
        r.box.Text(text);
        r.box.PlaceholderText(L"f(x)");
        r.box.FontFamily(winrt::FontFamily(L"Consolas"));
        r.box.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);
        r.box.MinWidth(220);

        r.preview = ui::Text(L"", 16, true);
        r.preview.Foreground(colorBrush);
        r.preview.Margin(winrt::Thickness{24, 0, 0, 0});

        auto box = r.box; auto preview = r.preview;
        r.box.TextChanged([this, box, preview](winrt::IInspectable const&, winrt::IInspectable const&) {
            SetPreview(box, preview);
            Render();
        });

        auto dydx = MakeOp(L"d/dx", L"Plot the derivative", box, [this, box] {
            std::string err; auto d = DifferentiateExpr(WideToUtf8(std::wstring(box.Text())), err);
            if (d) { AddRow(winrt::hstring(Utf8ToWide(*d))); Render(); }
            else status_.Text(winrt::hstring(L"\x26A0  " + Utf8ToWide(err)));
        });
        auto integ = MakeOp(L"\x222B dx", L"Plot the antiderivative", box, [this, box] {
            std::string err; auto i = IntegrateExpr(WideToUtf8(std::wstring(box.Text())), err);
            if (i) { AddRow(winrt::hstring(Utf8ToWide(*i + " + C"))); Render(); }
            else status_.Text(winrt::hstring(L"\x26A0  " + Utf8ToWide(err) +
                                             L" \x2014 try the numeric \x222B panel below"));
        });
        auto simp = MakeOp(L"simplify", L"Simplify in place", box, [this, box] {
            std::string err; auto s = SimplifyExpr(WideToUtf8(std::wstring(box.Text())), err);
            if (s) box.Text(winrt::hstring(Utf8ToWide(*s)));
        });

        auto remove = winrt::Button();
        remove.Content(winrt::box_value(winrt::hstring(L"\x2715")));
        remove.Click([this, box](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            for (size_t i = 0; i < rows_.size(); ++i) if (rows_[i].box == box) { rows_.erase(rows_.begin() + i); break; }
            Rebuild(); Render();
        });

        auto top = ui::HStack(8);
        top.VerticalAlignment(winrt::VerticalAlignment::Center);
        top.Children().Append(swatch);
        top.Children().Append(r.box);
        top.Children().Append(dydx);
        top.Children().Append(integ);
        top.Children().Append(simp);
        top.Children().Append(remove);

        auto wrap = ui::VStack(2);
        wrap.Children().Append(top);
        wrap.Children().Append(r.preview);

        rows_.push_back(r);
        exprPanel_.Children().Append(wrap);
        SetPreview(box, preview);
    }

    template <typename Fn>
    winrt::Button MakeOp(winrt::hstring label, winrt::hstring tip, winrt::TextBox, Fn fn) {
        winrt::Button b;
        b.Content(winrt::box_value(label));
        winrt::Controls::ToolTipService::SetToolTip(b, winrt::box_value(tip));
        b.Click([fn](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { fn(); });
        return b;
    }

    void Rebuild() {
        auto old = rows_;
        rows_.clear();
        exprPanel_.Children().Clear();
        for (auto& r : old) AddRow(r.box.Text());
    }

    void ComputeArea() {
        if (rows_.empty()) { areaLabel_.Text(L"add a function first"); return; }
        auto v = DefiniteIntegral(WideToUtf8(std::wstring(rows_[0].box.Text())), aBox_.Value(), bBox_.Value());
        if (v) { wchar_t b[64]; swprintf_s(b, L"= %.6g", *v); areaLabel_.Text(b); }
        else areaLabel_.Text(L"couldn't evaluate");
    }

    void Pan(double dxPx, double dyPx) {
        const double W = canvas_.ActualWidth(), H = canvas_.ActualHeight();
        if (W < 2 || H < 2) return;
        const double ux = (vxmax_ - vxmin_) / W, uy = (vymax_ - vymin_) / H;
        vxmin_ -= dxPx * ux; vxmax_ -= dxPx * ux;
        vymin_ += dyPx * uy; vymax_ += dyPx * uy;
        Render();
    }
    void Zoom(double factor, double cxPx, double cyPx) {
        const double W = canvas_.ActualWidth(), H = canvas_.ActualHeight();
        if (W < 2 || H < 2) return;
        const double cx = vxmin_ + (vxmax_ - vxmin_) * (cxPx / W);
        const double cy = vymin_ + (vymax_ - vymin_) * (1.0 - cyPx / H);
        vxmin_ = cx - (cx - vxmin_) * factor; vxmax_ = cx + (vxmax_ - cx) * factor;
        vymin_ = cy - (cy - vymin_) * factor; vymax_ = cy + (vymax_ - cy) * factor;
        Render();
    }

    winrt::Line MakeLine(double x1, double y1, double x2, double y2, winrt::Brush stroke, double thick) {
        winrt::Line l; l.X1(x1); l.Y1(y1); l.X2(x2); l.Y2(y2);
        l.Stroke(stroke); l.StrokeThickness(thick);
        return l;
    }
    void Label(double x, double y, winrt::hstring text, winrt::Brush brush) {
        auto t = ui::Text(text, 10.5);
        t.Foreground(brush);
        winrt::Canvas::SetLeft(t, x); winrt::Canvas::SetTop(t, y);
        canvas_.Children().Append(t);
    }

    void Render() {
        if (!canvas_) return;
        canvas_.Children().Clear();
        const double W = canvas_.ActualWidth(), H = canvas_.ActualHeight();
        if (W < 2 || H < 2 || !(vxmax_ > vxmin_) || !(vymax_ > vymin_)) return;

        auto sx = [&](double x) { return (x - vxmin_) / (vxmax_ - vxmin_) * W; };
        auto sy = [&](double y) { return H - (y - vymin_) / (vymax_ - vymin_) * H; };

        auto grid = ui::ThemeBrush(L"DividerStrokeColorDefaultBrush");
        if (!grid) grid = ui::ThemeBrush(L"TextFillColorTertiaryBrush");
        auto axis = ui::ThemeBrush(L"TextFillColorSecondaryBrush");
        auto labelBrush = ui::ThemeBrush(L"TextFillColorTertiaryBrush");

        const double stepX = NiceStep((vxmax_ - vxmin_) / (W / 80.0));
        const double stepY = NiceStep((vymax_ - vymin_) / (H / 60.0));
        for (double gx = std::ceil(vxmin_ / stepX) * stepX; gx <= vxmax_; gx += stepX) {
            const double px = sx(gx);
            if (grid) canvas_.Children().Append(MakeLine(px, 0, px, H, grid, 1));
            if (labelBrush && std::fabs(gx) > 1e-9) Label(px + 2, H - 16, winrt::hstring(FmtNum(gx)), labelBrush);
        }
        for (double gy = std::ceil(vymin_ / stepY) * stepY; gy <= vymax_; gy += stepY) {
            const double py = sy(gy);
            if (grid) canvas_.Children().Append(MakeLine(0, py, W, py, grid, 1));
            if (labelBrush && std::fabs(gy) > 1e-9) Label(2, py + 1, winrt::hstring(FmtNum(gy)), labelBrush);
        }
        if (axis) {
            if (vymin_ < 0 && vymax_ > 0) canvas_.Children().Append(MakeLine(0, sy(0), W, sy(0), axis, 1.5));
            if (vxmin_ < 0 && vxmax_ > 0) canvas_.Children().Append(MakeLine(sx(0), 0, sx(0), H, axis, 1.5));
        }

        for (auto& r : rows_) {
            std::string err;
            auto fn = CompileExpression(WideToUtf8(std::wstring(r.box.Text())), err);
            if (!fn) continue;
            winrt::SolidColorBrush brush{r.color};
            winrt::Polyline poly; poly.Stroke(brush); poly.StrokeThickness(2);
            bool have = false; double prevPy = 0;
            auto flush = [&] {
                if (poly.Points().Size() >= 2) canvas_.Children().Append(poly);
                poly = winrt::Polyline(); poly.Stroke(brush); poly.StrokeThickness(2);
            };
            const int cols = static_cast<int>(W);
            for (int i = 0; i <= cols; ++i) {
                const double x = vxmin_ + (vxmax_ - vxmin_) * i / cols;
                const double y = (*fn)(x);
                if (!std::isfinite(y)) { flush(); have = false; continue; }
                const double py = sy(y);
                if (have && std::fabs(py - prevPy) > H * 1.5) flush();
                poly.Points().Append(winrt::Point{static_cast<float>(sx(x)), static_cast<float>(py)});
                prevPy = py; have = true;
            }
            flush();
        }
    }

    winrt::Microsoft::UI::Xaml::Controls::StackPanel exprPanel_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock status_{nullptr}, areaLabel_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::NumberBox aBox_{nullptr}, bBox_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Canvas canvas_{nullptr};
    std::vector<Row> rows_;
    double vxmin_ = -10, vxmax_ = 10, vymin_ = -6, vymax_ = 6;
    bool dragging_ = false;
    double lastX_ = 0, lastY_ = 0;
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeGraphPage() {
    return std::make_unique<GraphPage>();
}

}  // namespace superwin
