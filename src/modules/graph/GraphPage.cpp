// Graphing Calculator page — a Desmos-style plotter with a built-in CAS:
//   * multiple colour-coded functions in a live "math field" that beautifies what
//     you type (x², √, ·, π, −) right in the input box — no preview line
//   * type calculus straight into the equation: d/dx(...), deriv(...), int(...),
//     integral(...), ∫(...)dx — it graphs the result (numeric fallback for ∫)
//   * drag to pan, wheel/buttons to zoom, hover for a live (x, y) trace
//   * a numeric definite-integral panel (Simpson's rule)
// Parsing / CAS live in Expr + GraphLogic (superwin_core, unit-tested); this file
// is the WinUI rendering + interaction.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
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
#include "modules/graph/MathField.h"

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

winrt::SolidColorBrush Brush(winrt::Windows::UI::Color c) { return winrt::SolidColorBrush{c}; }

struct Row {
    std::shared_ptr<MathField> field;
    winrt::Border swatch{nullptr};
    winrt::Grid container{nullptr};
    winrt::Windows::UI::Color color{};
    bool visible = true;
};

class GraphPage : public IModulePage {
public:
    GraphPage() { Build(); }
    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }

private:
    void Build() {
        exprPanel_ = ui::VStack(8);

        auto add = winrt::Button();
        add.Content(winrt::box_value(winrt::hstring(L"\x2795  Add function")));
        add.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { AddRow(L""); Render(); });

        auto controls = ui::HStack(8);
        controls.Children().Append(add);

        // Definite-integral panel.
        aBox_ = MakeNum(0); bBox_ = MakeNum(1);
        auto integ = winrt::Button();
        integ.Content(winrt::box_value(winrt::hstring(L"Compute \x222B")));
        integ.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { ComputeArea(); });
        areaLabel_ = ui::Text(L"", 14, true);
        auto areaRow = ui::HStack(8);
        areaRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        areaRow.Children().Append(ui::Text(L"\x222B of f\x2081 from", 13));
        areaRow.Children().Append(aBox_);
        areaRow.Children().Append(ui::Text(L"to", 13));
        areaRow.Children().Append(bBox_);
        areaRow.Children().Append(integ);
        areaRow.Children().Append(areaLabel_);

        status_ = ui::Caption(L"Drag to pan  \x2022  scroll or use \x002B / \x2212 to zoom  \x2022  hover to trace");

        // Plot surface: a Canvas with an overlaid zoom/home control cluster.
        canvas_ = winrt::Canvas();
        canvas_.Height(520);
        canvas_.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);
        if (auto bg = ui::ThemeBrush(L"CardBackgroundFillColorSecondaryBrush")) canvas_.Background(bg);
        canvas_.SizeChanged([this](winrt::IInspectable const&, winrt::SizeChangedEventArgs const&) { Render(); });
        canvas_.PointerPressed([this](winrt::IInspectable const&, winrt::PointerRoutedEventArgs const& e) {
            dragging_ = true; traceActive_ = false;
            auto p = e.GetCurrentPoint(canvas_).Position();
            lastX_ = p.X; lastY_ = p.Y;
            canvas_.CapturePointer(e.Pointer());
        });
        canvas_.PointerMoved([this](winrt::IInspectable const&, winrt::PointerRoutedEventArgs const& e) {
            auto p = e.GetCurrentPoint(canvas_).Position();
            if (dragging_) { Pan(p.X - lastX_, p.Y - lastY_); lastX_ = p.X; lastY_ = p.Y; }
            else { traceActive_ = true; traceX_ = p.X; traceY_ = p.Y; Render(); }
        });
        canvas_.PointerReleased([this](winrt::IInspectable const&, winrt::PointerRoutedEventArgs const& e) {
            dragging_ = false; canvas_.ReleasePointerCapture(e.Pointer());
        });
        canvas_.PointerExited([this](winrt::IInspectable const&, winrt::PointerRoutedEventArgs const&) {
            if (traceActive_) { traceActive_ = false; Render(); }
        });
        canvas_.PointerWheelChanged([this](winrt::IInspectable const&, winrt::PointerRoutedEventArgs const& e) {
            auto pt = e.GetCurrentPoint(canvas_);
            const int delta = pt.Properties().MouseWheelDelta();
            Zoom(delta > 0 ? 0.85 : 1.0 / 0.85, pt.Position().X, pt.Position().Y);
        });

        auto zoomBox = ui::VStack(6);
        zoomBox.HorizontalAlignment(winrt::HorizontalAlignment::Right);
        zoomBox.VerticalAlignment(winrt::VerticalAlignment::Top);
        zoomBox.Margin(winrt::Thickness{0, 12, 12, 0});
        zoomBox.Children().Append(OverlayButton(L"\x002B", [this] { ZoomCenter(0.8); }));
        zoomBox.Children().Append(OverlayButton(L"\x2212", [this] { ZoomCenter(1.0 / 0.8); }));
        zoomBox.Children().Append(OverlayButton(L"\x2302", [this] { ResetView(); }));

        winrt::Grid plot;
        plot.Children().Append(canvas_);
        plot.Children().Append(zoomBox);

        auto card = ui::VStack(12);
        card.Children().Append(ui::Text(L"Functions", 16, true));
        card.Children().Append(exprPanel_);
        card.Children().Append(controls);
        card.Children().Append(ui::Card(areaRow, 12));
        card.Children().Append(status_);
        card.Children().Append(ui::Card(plot, 0));

        root_ = ui::Page(L"Graphing Calculator", ui::Card(card));

        AddRow(L"sin(x)");
        Render();
    }

    winrt::Button OverlayButton(winrt::hstring glyph, std::function<void()> fn) {
        winrt::Button b;
        b.Content(winrt::box_value(glyph));
        b.Width(38); b.Height(38);
        b.FontSize(17);
        b.Padding(winrt::Thickness{0, 0, 0, 0});
        b.Click([fn](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { fn(); });
        return b;
    }

    winrt::NumberBox MakeNum(double v) {
        winrt::NumberBox n;
        n.Value(v); n.SmallChange(1);
        n.SpinButtonPlacementMode(winrt::NumberBoxSpinButtonPlacementMode::Inline);
        n.Width(120);
        return n;
    }

    winrt::ColumnDefinition Col(double v, winrt::GridUnitType type) {
        winrt::ColumnDefinition c;
        c.Width(winrt::GridLengthHelper::FromValueAndType(v, type));
        return c;
    }

    void AddRow(winrt::hstring text) {
        Row r;
        r.color = kPalette[rows_.size() % (sizeof(kPalette) / sizeof(kPalette[0]))];
        auto colorBrush = Brush(r.color);

        r.swatch = winrt::Border();
        r.swatch.Width(16); r.swatch.Height(16);
        r.swatch.CornerRadius(winrt::CornerRadius{8, 8, 8, 8});
        r.swatch.Background(colorBrush);
        r.swatch.BorderBrush(colorBrush);
        r.swatch.BorderThickness(winrt::Thickness{2, 2, 2, 2});
        r.swatch.VerticalAlignment(winrt::VerticalAlignment::Center);
        winrt::Controls::ToolTipService::SetToolTip(r.swatch, winrt::box_value(winrt::hstring(L"Show / hide")));

        r.field = std::make_shared<MathField>();
        r.field->SetForeground(colorBrush);
        r.field->OnChanged([this] { Render(); });

        auto field = r.field;
        auto dydx = MakeOp(L"d/dx", L"Plot the derivative", [this, field] {
            std::string err; auto d = DifferentiateExpr(field->ToAscii(), err);
            if (d) { AddRow(winrt::hstring(Utf8ToWide(*d))); Render(); }
            else status_.Text(winrt::hstring(L"\x26A0  " + Utf8ToWide(err)));
        });
        auto integ = MakeOp(L"\x222B dx", L"Plot the antiderivative", [this, field] {
            // Route through the typed int(...) operator so it always graphs (symbolic
            // when there's a closed form, numeric ∫₀ˣ otherwise).
            const std::string a = field->ToAscii();
            if (!a.empty()) { AddRow(winrt::hstring(Utf8ToWide("int(" + a + ")"))); Render(); }
        });
        auto simp = MakeOp(L"simplify", L"Simplify in place", [field] {
            std::string err; auto s = SimplifyExpr(field->ToAscii(), err);
            if (s) field->SetText(*s);
        });

        auto remove = winrt::Button();
        remove.Content(winrt::box_value(winrt::hstring(L"\x2715")));
        remove.Click([this, field](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            RemoveRow(field);
        });

        auto ops = ui::HStack(6);
        ops.VerticalAlignment(winrt::VerticalAlignment::Center);
        ops.Children().Append(dydx);
        ops.Children().Append(integ);
        ops.Children().Append(simp);
        ops.Children().Append(remove);

        r.container = winrt::Grid();
        r.container.ColumnDefinitions().Append(Col(0, winrt::GridUnitType::Auto));
        r.container.ColumnDefinitions().Append(Col(1, winrt::GridUnitType::Star));
        r.container.ColumnDefinitions().Append(Col(0, winrt::GridUnitType::Auto));
        r.container.ColumnSpacing(10);
        r.container.VerticalAlignment(winrt::VerticalAlignment::Center);
        auto fieldCtl = r.field->Control();
        winrt::Grid::SetColumn(r.swatch, 0);
        winrt::Grid::SetColumn(fieldCtl, 1);
        winrt::Grid::SetColumn(ops, 2);
        r.container.Children().Append(r.swatch);
        r.container.Children().Append(fieldCtl);
        r.container.Children().Append(ops);

        // Toggle visibility by tapping the colour swatch.
        auto swatch = r.swatch; auto fld = r.field;
        r.swatch.Tapped([this, fld, swatch](winrt::IInspectable const&, winrt::TappedRoutedEventArgs const&) {
            for (auto& row : rows_) if (row.field == fld) {
                row.visible = !row.visible;
                swatch.Background(row.visible ? Brush(row.color) : winrt::SolidColorBrush{winrt::Windows::UI::Color{0,0,0,0}});
                swatch.Opacity(row.visible ? 1.0 : 0.9);
                break;
            }
            Render();
        });

        rows_.push_back(r);
        exprPanel_.Children().Append(r.container);
        r.field->SetText(WideToUtf8(std::wstring(text)));
    }

    template <typename Fn>
    winrt::Button MakeOp(winrt::hstring label, winrt::hstring tip, Fn fn) {
        winrt::Button b;
        b.Content(winrt::box_value(label));
        b.FontSize(12.5);
        winrt::Controls::ToolTipService::SetToolTip(b, winrt::box_value(tip));
        b.Click([fn](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { fn(); });
        return b;
    }

    void RemoveRow(std::shared_ptr<MathField> field) {
        for (size_t i = 0; i < rows_.size(); ++i) {
            if (rows_[i].field != field) continue;
            auto container = rows_[i].container;
            auto kids = exprPanel_.Children();
            for (uint32_t j = 0; j < kids.Size(); ++j) {
                if (kids.GetAt(j).try_as<winrt::Grid>() == container) { kids.RemoveAt(j); break; }
            }
            rows_.erase(rows_.begin() + i);
            break;
        }
        Render();
    }

    void ComputeArea() {
        if (rows_.empty()) { areaLabel_.Text(L"add a function first"); return; }
        auto v = DefiniteIntegral(rows_[0].field->ToAscii(), aBox_.Value(), bBox_.Value());
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
    void ZoomCenter(double factor) {
        Zoom(factor, canvas_.ActualWidth() / 2, canvas_.ActualHeight() / 2);
    }
    void ResetView() { vxmin_ = -10; vxmax_ = 10; vymin_ = -6; vymax_ = 6; Render(); }

    winrt::Line MakeLine(double x1, double y1, double x2, double y2, winrt::Brush stroke, double thick, double opacity = 1.0) {
        winrt::Line l; l.X1(x1); l.Y1(y1); l.X2(x2); l.Y2(y2);
        l.Stroke(stroke); l.StrokeThickness(thick); l.Opacity(opacity);
        return l;
    }
    void Label(double x, double y, winrt::hstring text, winrt::Brush brush) {
        auto t = ui::Text(text, 11);
        t.Foreground(brush);
        winrt::Canvas::SetLeft(t, x); winrt::Canvas::SetTop(t, y);
        canvas_.Children().Append(t);
    }

    void Render() {
        if (!canvas_) return;
        canvas_.Children().Clear();
        const double W = canvas_.ActualWidth(), H = canvas_.ActualHeight();
        if (W < 2 || H < 2 || !(vxmax_ > vxmin_) || !(vymax_ > vymin_)) return;

        // Clip curves to the plot rectangle (it sits inside a rounded card).
        winrt::RectangleGeometry clip;
        clip.Rect(winrt::Rect{0, 0, static_cast<float>(W), static_cast<float>(H)});
        canvas_.Clip(clip);

        auto sx = [&](double x) { return (x - vxmin_) / (vxmax_ - vxmin_) * W; };
        auto sy = [&](double y) { return H - (y - vymin_) / (vymax_ - vymin_) * H; };

        auto grid = ui::ThemeBrush(L"DividerStrokeColorDefaultBrush");
        if (!grid) grid = ui::ThemeBrush(L"TextFillColorTertiaryBrush");
        auto axis = ui::ThemeBrush(L"TextFillColorSecondaryBrush");
        auto labelBrush = ui::ThemeBrush(L"TextFillColorTertiaryBrush");

        const double stepX = NiceStep((vxmax_ - vxmin_) / (W / 84.0));
        const double stepY = NiceStep((vymax_ - vymin_) / (H / 64.0));

        // Minor grid (subtle), then major grid + labels.
        const double minorX = stepX / 5, minorY = stepY / 5;
        if (grid) {
            for (double gx = std::ceil(vxmin_ / minorX) * minorX; gx <= vxmax_; gx += minorX)
                canvas_.Children().Append(MakeLine(sx(gx), 0, sx(gx), H, grid, 1, 0.30));
            for (double gy = std::ceil(vymin_ / minorY) * minorY; gy <= vymax_; gy += minorY)
                canvas_.Children().Append(MakeLine(0, sy(gy), W, sy(gy), grid, 1, 0.30));
        }
        for (double gx = std::ceil(vxmin_ / stepX) * stepX; gx <= vxmax_; gx += stepX) {
            const double px = sx(gx);
            if (grid) canvas_.Children().Append(MakeLine(px, 0, px, H, grid, 1, 0.85));
            if (labelBrush && std::fabs(gx) > 1e-9) Label(px + 3, H - 17, winrt::hstring(FmtNum(gx)), labelBrush);
        }
        for (double gy = std::ceil(vymin_ / stepY) * stepY; gy <= vymax_; gy += stepY) {
            const double py = sy(gy);
            if (grid) canvas_.Children().Append(MakeLine(0, py, W, py, grid, 1, 0.85));
            if (labelBrush && std::fabs(gy) > 1e-9) Label(3, py + 1, winrt::hstring(FmtNum(gy)), labelBrush);
        }
        if (axis) {
            if (vymin_ < 0 && vymax_ > 0) canvas_.Children().Append(MakeLine(0, sy(0), W, sy(0), axis, 1.8));
            if (vxmin_ < 0 && vxmax_ > 0) canvas_.Children().Append(MakeLine(sx(0), 0, sx(0), H, axis, 1.8));
        }

        for (auto& r : rows_) {
            if (!r.visible) continue;
            std::string err;
            auto fn = CompileExpression(r.field->ToAscii(), err);
            if (!fn) continue;
            auto brush = Brush(r.color);
            winrt::Polyline poly; poly.Stroke(brush); poly.StrokeThickness(2.6);
            poly.StrokeLineJoin(winrt::PenLineJoin::Round);
            bool have = false; double prevPy = 0;
            auto flush = [&] {
                if (poly.Points().Size() >= 2) canvas_.Children().Append(poly);
                poly = winrt::Polyline(); poly.Stroke(brush); poly.StrokeThickness(2.6);
                poly.StrokeLineJoin(winrt::PenLineJoin::Round);
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

        DrawTrace(W, H, sy, axis);
    }

    template <typename SY>
    void DrawTrace(double W, double H, SY sy, winrt::Brush axis) {
        if (!traceActive_ || traceX_ < 0 || traceX_ > W) return;
        const double dataX = vxmin_ + (vxmax_ - vxmin_) * (traceX_ / W);

        // Snap to the visible curve whose point is nearest the cursor.
        double bestPy = 0, bestY = 0, bestDist = std::numeric_limits<double>::infinity();
        winrt::Windows::UI::Color bestColor{};
        bool found = false;
        for (auto& r : rows_) {
            if (!r.visible) continue;
            std::string err;
            auto fn = CompileExpression(r.field->ToAscii(), err);
            if (!fn) continue;
            const double y = (*fn)(dataX);
            if (!std::isfinite(y)) continue;
            const double py = sy(y);
            const double d = std::fabs(py - traceY_);
            if (d < bestDist) { bestDist = d; bestPy = py; bestY = y; bestColor = r.color; found = true; }
        }

        // Vertical guide line at the cursor x.
        if (axis) canvas_.Children().Append(MakeLine(traceX_, 0, traceX_, H, axis, 1, 0.45));
        if (!found) return;

        winrt::Ellipse dot;
        dot.Width(11); dot.Height(11);
        dot.Fill(Brush(bestColor));
        dot.Stroke(Brush(winrt::Windows::UI::Color{255, 255, 255, 255}));
        dot.StrokeThickness(2);
        winrt::Canvas::SetLeft(dot, traceX_ - 5.5);
        winrt::Canvas::SetTop(dot, bestPy - 5.5);
        canvas_.Children().Append(dot);

        wchar_t buf[64];
        swprintf_s(buf, L"(%.3g, %.3g)", dataX, bestY);
        auto txt = ui::Text(buf, 12.5, true);
        txt.Foreground(Brush(winrt::Windows::UI::Color{255, 255, 255, 255}));
        winrt::Border pill;
        pill.Background(Brush(bestColor));
        pill.CornerRadius(winrt::CornerRadius{6, 6, 6, 6});
        pill.Padding(winrt::Thickness{8, 3, 8, 3});
        pill.Child(txt);
        double lx = traceX_ + 12, ly = bestPy - 30;
        if (lx > W - 130) lx = traceX_ - 130;
        if (ly < 4) ly = bestPy + 14;
        winrt::Canvas::SetLeft(pill, lx);
        winrt::Canvas::SetTop(pill, ly);
        canvas_.Children().Append(pill);
    }

    winrt::Microsoft::UI::Xaml::Controls::StackPanel exprPanel_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock status_{nullptr}, areaLabel_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::NumberBox aBox_{nullptr}, bBox_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Canvas canvas_{nullptr};
    std::vector<Row> rows_;
    double vxmin_ = -10, vxmax_ = 10, vymin_ = -6, vymax_ = 6;
    bool dragging_ = false;
    double lastX_ = 0, lastY_ = 0;
    bool traceActive_ = false;
    double traceX_ = 0, traceY_ = 0;
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeGraphPage() {
    return std::make_unique<GraphPage>();
}

}  // namespace superwin
