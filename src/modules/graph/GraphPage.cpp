// Graphing Calculator page — a TI-Nspire CX II CAS-style plotter.
//
// Layout: an editable LaTeX equation list on the LEFT (a WebView2 hosting MathLive)
// and the plot on the RIGHT (a native Canvas). MathLive renders real ∫ / Σ / Π /
// fractions / roman sin·cos as you type; on every edit the page posts the rows'
// LaTeX to us, we translate it to the CAS grammar (Latex.h -> Expr.h) and plot.
// If the WebView2 runtime is unavailable we fall back to a plain text editor.
//
// Parsing / CAS live in Expr + Latex + GraphLogic (superwin_core, unit-tested).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <Windows.h>

#include <nlohmann/json.hpp>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.Web.WebView2.Core.h>

#include "app/Ui.h"
#include "core/Strings.h"
#include "modules/calc/CalcLogic.h"
#include "modules/graph/Cas.h"
#include "modules/graph/Expr.h"
#include "modules/graph/GraphLogic.h"
#include "modules/graph/Latex.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Input;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Shapes;
namespace wv2 = winrt::Microsoft::Web::WebView2::Core;
}  // namespace winrt

namespace superwin {
namespace {

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

winrt::Windows::UI::Color HexColor(const std::string& h) {
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return (c >= 'a' && c <= 'f') ? c - 'a' + 10 : 0;
    };
    winrt::Windows::UI::Color c{255, 0x4f, 0x8e, 0xf7};
    if (h.size() >= 7 && h[0] == '#') {
        c.R = static_cast<uint8_t>(nib(h[1]) * 16 + nib(h[2]));
        c.G = static_cast<uint8_t>(nib(h[3]) * 16 + nib(h[4]));
        c.B = static_cast<uint8_t>(nib(h[5]) * 16 + nib(h[6]));
    }
    return c;
}

std::wstring ExeDir() {
    wchar_t b[MAX_PATH]{};
    ::GetModuleFileNameW(nullptr, b, MAX_PATH);
    std::wstring p = b;
    const auto i = p.find_last_of(L"\\/");
    return i == std::wstring::npos ? L"." : p.substr(0, i);
}

struct PlotItem {
    std::string infix;
    winrt::Windows::UI::Color color{};
    bool visible = true;
};

// A compact symbolic-algebra console, shown only in the Class IV CAS calculator.
// Type an expression in x and Simplify it, take d/dx, ∫dx, Solve f(x)=0, or
// evaluate at a point -- driven by the Expr/Cas engine in this module.
class CasPanel {
public:
    CasPanel() { Build(); }
    winrt::UIElement Root() { return root_; }

private:
    void Build() {
        auto col = ui::VStack(10);

        input_ = winrt::TextBox();
        input_.PlaceholderText(winrt::hstring(L"f(x)   e.g.   x^2 - 4    sin(x)    d/dx(x^3)    (x+1)(x-2)"));
        input_.FontSize(17);
        input_.FontFamily(winrt::Media::FontFamily(L"Consolas"));
        input_.AcceptsReturn(false);
        auto inCol = ui::VStack(6);
        inCol.Children().Append(ui::Caption(L"CAS \x2014 expression in x"));
        inCol.Children().Append(input_);
        col.Children().Append(inCol);

        auto actions = ui::HStack(8);
        actions.Children().Append(ActionButton(L"Simplify", [this] { DoSimplify(); }));
        actions.Children().Append(ActionButton(L"d/dx", [this] { DoDeriv(); }));
        actions.Children().Append(ActionButton(L"\x222B dx", [this] { DoIntegral(); }));
        actions.Children().Append(ActionButton(L"Solve = 0", [this] { DoSolve(); }));
        col.Children().Append(actions);

        auto evalRow = ui::HStack(8);
        evalRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        auto el = ui::Caption(L"Evaluate at x =");
        el.VerticalAlignment(winrt::VerticalAlignment::Center);
        evalX_ = winrt::TextBox();
        evalX_.Width(120);
        evalX_.PlaceholderText(winrt::hstring(L"0"));
        evalRow.Children().Append(el);
        evalRow.Children().Append(evalX_);
        evalRow.Children().Append(ActionButton(L"Evaluate", [this] { DoEval(); }, false));
        col.Children().Append(evalRow);

        outText_ = ui::Text(L"", 20, true);
        outText_.FontFamily(winrt::Media::FontFamily(L"Cambria Math"));
        outText_.IsTextSelectionEnabled(true);
        auto outCol = ui::VStack(6);
        outCol.Children().Append(ui::Caption(L"Result"));
        outCol.Children().Append(outText_);
        col.Children().Append(outCol);

        auto card = ui::Card(col, 16);
        root_ = card;
    }

    winrt::Button ActionButton(winrt::hstring label, std::function<void()> fn, bool accent = true) {
        winrt::Button b;
        b.Content(winrt::box_value(label));
        b.MinWidth(96);
        b.MinHeight(38);
        b.FontSize(15);
        b.CornerRadius(winrt::CornerRadius{10, 10, 10, 10});
        if (accent) {
            if (auto st = winrt::Application::Current().Resources()
                              .TryLookup(winrt::box_value(winrt::hstring(L"AccentButtonStyle")))
                              .try_as<winrt::Style>())
                b.Style(st);
        }
        b.Click([fn](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { fn(); });
        return b;
    }

    std::optional<Expr> Parse(std::string& err) {
        const std::string in = WideToUtf8(std::wstring(input_.Text()));
        if (in.find_first_not_of(" \t") == std::string::npos) { err = "enter an expression in x"; return std::nullopt; }
        return ParseExpr(in, err);
    }
    void Show(const std::wstring& s) {
        if (auto b = ui::ThemeBrush(L"TextFillColorPrimaryBrush")) outText_.Foreground(b);
        outText_.Text(winrt::hstring(s));
    }
    void ShowError(const std::string& err) {
        if (auto b = ui::ThemeBrush(L"SystemFillColorCriticalBrush")) outText_.Foreground(b);
        outText_.Text(winrt::hstring(L"\x26A0  " + Utf8ToWide(err)));
    }
    void DoSimplify() {
        std::string err; auto e = Parse(err);
        if (!e) { ShowError(err); return; }
        Show(Utf8ToWide(e->simplify().pretty()));
    }
    void DoDeriv() {
        std::string err; auto e = Parse(err);
        if (!e) { ShowError(err); return; }
        Show(L"d/dx = " + Utf8ToWide(e->derivative().simplify().pretty()));
    }
    void DoIntegral() {
        std::string err; auto e = Parse(err);
        if (!e) { ShowError(err); return; }
        auto i = e->integral();
        if (i.valid()) Show(L"\x222B = " + Utf8ToWide(i.pretty()) + L" + C");
        else Show(L"No elementary antiderivative \x2014 plot the numeric \x222B instead.");
    }
    void DoSolve() {
        std::string err; auto e = Parse(err);
        if (!e) { ShowError(err); return; }
        auto roots = SolveRoots(*e);
        if (roots.empty()) { Show(L"No real roots in [\x2212" L"100, 100]."); return; }
        std::wstring s;
        for (size_t i = 0; i < roots.size(); ++i) {
            if (i) s += L",   ";
            s += L"x = " + Utf8ToWide(FormatCalc(roots[i]));
        }
        Show(s);
    }
    void DoEval() {
        std::string err; auto e = Parse(err);
        if (!e) { ShowError(err); return; }
        std::string xs = WideToUtf8(std::wstring(evalX_.Text()));
        if (xs.find_first_not_of(" \t") == std::string::npos) xs = "0";
        std::string xerr;
        auto xv = EvaluateCalc(xs, AngleMode::Radians, xerr);
        if (!xv) { Show(L"Enter a numeric x value."); return; }
        const double y = e->eval(*xv);
        Show(L"f(" + Utf8ToWide(FormatCalc(*xv)) + L") = " + Utf8ToWide(FormatCalc(y)));
    }

    winrt::UIElement root_{nullptr};
    winrt::TextBox input_{nullptr};
    winrt::TextBox evalX_{nullptr};
    winrt::TextBlock outText_{nullptr};
};

class GraphPage : public IModulePage {
public:
    explicit GraphPage(bool cas) : cas_(cas) { Build(); }
    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }

private:
    void Build() {
        // ---- right: the plot ----
        canvas_ = winrt::Canvas();
        canvas_.MinHeight(560);
        canvas_.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);
        canvas_.VerticalAlignment(winrt::VerticalAlignment::Stretch);
        canvas_.SizeChanged([this](winrt::IInspectable const&, winrt::SizeChangedEventArgs const&) { Render(); });
        canvas_.ActualThemeChanged([this](winrt::FrameworkElement const&, winrt::IInspectable const&) { PushTheme(); Render(); });
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
            Zoom(pt.Properties().MouseWheelDelta() > 0 ? 0.85 : 1.0 / 0.85, pt.Position().X, pt.Position().Y);
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
        plot.MinHeight(560);
        auto plotCard = ui::Card(plot, 0);

        // ---- left: the equation editor (WebView2 + MathLive) ----
        leftHost_ = winrt::Border();
        leftHost_.CornerRadius(winrt::CornerRadius{12, 12, 12, 12});
        leftHost_.Width(370);
        leftHost_.MinHeight(560);
        if (auto bg = ui::ThemeBrush(L"CardBackgroundFillColorDefaultBrush")) leftHost_.Background(bg);
        if (auto st = ui::ThemeBrush(L"CardStrokeColorDefaultBrush")) {
            leftHost_.BorderBrush(st);
            leftHost_.BorderThickness(winrt::Thickness{1, 1, 1, 1});
        }
        SetupWebView();

        // ---- split: editor left, plot right ----
        winrt::Grid split;
        auto cEd = winrt::ColumnDefinition(); cEd.Width(winrt::GridLengthHelper::Auto());
        auto cPl = winrt::ColumnDefinition(); cPl.Width(winrt::GridLengthHelper::FromValueAndType(1, winrt::GridUnitType::Star));
        split.ColumnDefinitions().Append(cEd);
        split.ColumnDefinitions().Append(cPl);
        split.ColumnSpacing(14);
        winrt::Grid::SetColumn(leftHost_, 0);
        winrt::Grid::SetColumn(plotCard, 1);
        split.Children().Append(leftHost_);
        split.Children().Append(plotCard);

        status_ = ui::Caption(cas_
            ? L"Type math on the left \x2014 \x222B, \x03A3, \x03A0, d/dx and fractions render live  \x2022  drag to pan, scroll to zoom  \x2022  use the CAS console below for symbolic algebra"
            : L"Type math on the left \x2014 fractions, roots, powers and trig render live  \x2022  drag to pan, scroll to zoom");

        auto body = ui::VStack(12);
        body.Children().Append(split);
        body.Children().Append(status_);

        // Class IV CAS adds a symbolic-algebra console under the plot.
        if (cas_) {
            casConsole_ = std::make_unique<CasPanel>();
            body.Children().Append(casConsole_->Root());
        }

        root_ = ui::Page(cas_ ? L"CAS Graphing Calculator" : L"Graphing Calculator", body);
    }

    void SetupWebView() {
        webview_ = winrt::WebView2();
        webview_.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);
        webview_.VerticalAlignment(winrt::VerticalAlignment::Stretch);
        leftHost_.Child(webview_);

        webview_.CoreWebView2Initialized([this](winrt::WebView2 const&, winrt::CoreWebView2InitializedEventArgs const& args) {
            if (args.Exception()) { ShowWebFallback(); return; }
            auto core = webview_.CoreWebView2();
            auto s = core.Settings();
            s.AreDevToolsEnabled(false);
            s.AreDefaultContextMenusEnabled(false);
            s.IsZoomControlEnabled(false);
            s.IsStatusBarEnabled(false);
            core.SetVirtualHostNameToFolderMapping(
                L"superwin.graph", ExeDir() + L"\\web", winrt::wv2::CoreWebView2HostResourceAccessKind::Allow);
            core.WebMessageReceived([this](winrt::wv2::CoreWebView2 const&, winrt::wv2::CoreWebView2WebMessageReceivedEventArgs const& e) {
                OnWebMessage(e);
            });
            webview_.NavigationCompleted([this](winrt::WebView2 const&, winrt::wv2::CoreWebView2NavigationCompletedEventArgs const&) {
                PushTheme();
            });
            core.Navigate(L"https://superwin.graph/index.html");
        });

        // Kick off initialization; failures surface via the event's Exception().
        try { webview_.EnsureCoreWebView2Async(); }
        catch (...) { ShowWebFallback(); }
    }

    void ShowWebFallback() {
        auto col = ui::VStack(8);
        col.Margin(winrt::Thickness{16, 16, 16, 16});
        col.Children().Append(ui::Text(L"WebView2 runtime not found", 15, true));
        col.Children().Append(ui::Caption(
            L"The live LaTeX editor needs the Microsoft Edge WebView2 runtime (preinstalled on "
            L"Windows 11). Install it, then reopen SuperWin."));
        leftHost_.Child(col);
    }

    void OnWebMessage(winrt::wv2::CoreWebView2WebMessageReceivedEventArgs const& e) {
        std::string utf8;
        try { utf8 = WideToUtf8(std::wstring(e.TryGetWebMessageAsString())); }
        catch (...) { return; }
        auto j = nlohmann::json::parse(utf8, nullptr, false);
        if (j.is_discarded() || !j.is_object()) return;
        if (j.value("type", std::string()) != "state" || !j.contains("rows")) return;

        items_.clear();
        for (auto const& r : j["rows"]) {
            PlotItem it;
            it.infix = LatexToInfix(r.value("latex", std::string()));
            it.color = HexColor(r.value("color", std::string("#4f8ef7")));
            it.visible = r.value("visible", true);
            items_.push_back(std::move(it));
        }
        Render();
    }

    void PushTheme() {
        if (!webview_ || !webview_.CoreWebView2()) return;
        const bool dark = leftHost_ && leftHost_.ActualTheme() == winrt::ElementTheme::Dark;
        webview_.ExecuteScriptAsync(dark ? L"swSetTheme('dark')" : L"swSetTheme('light')");
        // Class III is graphing-only: hide the symbolic (CAS) insert buttons.
        webview_.ExecuteScriptAsync(cas_ ? L"swSetCas(true)" : L"swSetCas(false)");
    }

    winrt::Button OverlayButton(winrt::hstring glyph, std::function<void()> fn) {
        winrt::Button b;
        b.Content(winrt::box_value(glyph));
        b.Width(38); b.Height(38); b.FontSize(17);
        b.Padding(winrt::Thickness{0, 0, 0, 0});
        b.Click([fn](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { fn(); });
        return b;
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
    void ZoomCenter(double factor) { Zoom(factor, canvas_.ActualWidth() / 2, canvas_.ActualHeight() / 2); }
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

        winrt::RectangleGeometry clip;
        clip.Rect(winrt::Rect{0, 0, static_cast<float>(W), static_cast<float>(H)});
        canvas_.Clip(clip);

        auto sx = [&](double x) { return (x - vxmin_) / (vxmax_ - vxmin_) * W; };
        auto sy = [&](double y) { return H - (y - vymin_) / (vymax_ - vymin_) * H; };

        // TI-Nspire-style colour screen: light plot + crisp grid, dark equivalent.
        const bool dark = canvas_.ActualTheme() == winrt::ElementTheme::Dark;
        auto rgba = [](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
            return winrt::SolidColorBrush{winrt::Windows::UI::Color{a, r, g, b}};
        };
        canvas_.Background(dark ? rgba(22, 24, 29, 255) : rgba(255, 255, 255, 255));
        auto minorBrush = dark ? rgba(255, 255, 255, 20) : rgba(0, 0, 0, 16);
        auto majorBrush = dark ? rgba(255, 255, 255, 46) : rgba(0, 0, 0, 38);
        winrt::Brush axis = dark ? rgba(255, 255, 255, 170) : rgba(20, 20, 24, 180);
        winrt::Brush labelBrush = dark ? rgba(225, 225, 230, 200) : rgba(60, 60, 66, 220);

        const double stepX = NiceStep((vxmax_ - vxmin_) / (W / 84.0));
        const double stepY = NiceStep((vymax_ - vymin_) / (H / 64.0));
        const double minorX = stepX / 5, minorY = stepY / 5;
        for (double gx = std::ceil(vxmin_ / minorX) * minorX; gx <= vxmax_; gx += minorX)
            canvas_.Children().Append(MakeLine(sx(gx), 0, sx(gx), H, minorBrush, 1));
        for (double gy = std::ceil(vymin_ / minorY) * minorY; gy <= vymax_; gy += minorY)
            canvas_.Children().Append(MakeLine(0, sy(gy), W, sy(gy), minorBrush, 1));
        for (double gx = std::ceil(vxmin_ / stepX) * stepX; gx <= vxmax_; gx += stepX) {
            const double px = sx(gx);
            canvas_.Children().Append(MakeLine(px, 0, px, H, majorBrush, 1));
            if (std::fabs(gx) > 1e-9) Label(px + 3, H - 17, winrt::hstring(FmtNum(gx)), labelBrush);
        }
        for (double gy = std::ceil(vymin_ / stepY) * stepY; gy <= vymax_; gy += stepY) {
            const double py = sy(gy);
            canvas_.Children().Append(MakeLine(0, py, W, py, majorBrush, 1));
            if (std::fabs(gy) > 1e-9) Label(3, py + 1, winrt::hstring(FmtNum(gy)), labelBrush);
        }
        if (vymin_ < 0 && vymax_ > 0) canvas_.Children().Append(MakeLine(0, sy(0), W, sy(0), axis, 1.8));
        if (vxmin_ < 0 && vxmax_ > 0) canvas_.Children().Append(MakeLine(sx(0), 0, sx(0), H, axis, 1.8));

        for (auto const& item : items_) {
            if (!item.visible || item.infix.empty()) continue;
            std::string err;
            auto fn = CompileExpression(item.infix, err);
            if (!fn) continue;
            auto brush = Brush(item.color);
            winrt::Polyline poly; poly.Stroke(brush); poly.StrokeThickness(2.6);
            poly.StrokeLineJoin(winrt::PenLineJoin::Round);
            bool have = false, prevAbove = false, prevBelow = false;
            auto flush = [&] {
                if (poly.Points().Size() >= 2) canvas_.Children().Append(poly);
                poly = winrt::Polyline(); poly.Stroke(brush); poly.StrokeThickness(2.6);
                poly.StrokeLineJoin(winrt::PenLineJoin::Round);
            };
            // Clamp y to one extra viewport of headroom so a steep-but-continuous
            // curve (x^3 .. x^n) stays a single connected polyline that simply runs
            // off the top/bottom, instead of being shattered into <2-point segments
            // that never draw. A genuine asymptote (1/x, tan x) teleports from above
            // the screen straight to below it with no on-screen sample in between --
            // that, and only that, breaks the line so we don't draw a false vertical.
            const int cols = static_cast<int>(W);
            const double pad = vymax_ - vymin_;
            const double yLo = vymin_ - pad, yHi = vymax_ + pad;
            for (int i = 0; i <= cols; ++i) {
                const double x = vxmin_ + (vxmax_ - vxmin_) * i / cols;
                const double y = (*fn)(x);
                if (!std::isfinite(y)) { flush(); have = false; continue; }
                const bool above = y > vymax_, below = y < vymin_;
                if (have && ((above && prevBelow) || (below && prevAbove))) { flush(); have = false; }
                const double yc = y < yLo ? yLo : (y > yHi ? yHi : y);
                poly.Points().Append(winrt::Point{static_cast<float>(sx(x)), static_cast<float>(sy(yc))});
                prevAbove = above; prevBelow = below; have = true;
            }
            flush();
        }

        DrawTrace(W, H, sy, axis);
    }

    template <typename SY>
    void DrawTrace(double W, double H, SY sy, winrt::Brush axis) {
        if (!traceActive_ || traceX_ < 0 || traceX_ > W) return;
        const double dataX = vxmin_ + (vxmax_ - vxmin_) * (traceX_ / W);
        double bestPy = 0, bestY = 0, bestDist = std::numeric_limits<double>::infinity();
        winrt::Windows::UI::Color bestColor{};
        bool found = false;
        for (auto const& item : items_) {
            if (!item.visible || item.infix.empty()) continue;
            std::string err;
            auto fn = CompileExpression(item.infix, err);
            if (!fn) continue;
            const double y = (*fn)(dataX);
            if (!std::isfinite(y)) continue;
            const double py = sy(y);
            const double d = std::fabs(py - traceY_);
            if (d < bestDist) { bestDist = d; bestPy = py; bestY = y; bestColor = item.color; found = true; }
        }
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

    winrt::Microsoft::UI::Xaml::Controls::WebView2 webview_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Border leftHost_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock status_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Canvas canvas_{nullptr};
    std::vector<PlotItem> items_;
    double vxmin_ = -10, vxmax_ = 10, vymin_ = -6, vymax_ = 6;
    bool dragging_ = false;
    double lastX_ = 0, lastY_ = 0;
    bool traceActive_ = false;
    double traceX_ = 0, traceY_ = 0;
    bool cas_ = true;  // Class IV (CAS) vs Class III (graphing-only)
    std::unique_ptr<CasPanel> casConsole_;
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeGraphPage(bool cas) {
    return std::make_unique<GraphPage>(cas);
}

}  // namespace superwin
