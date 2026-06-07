// Calculator module: one page with four tabs -- Class I (basic), Class II
// (scientific), Class III (advanced) and Class IV CAS (the graphing/CAS engine).
// All four share the same sleek keypad look; a feature spec decides which keys and
// modes each class exposes. Class I & II offer a "Non-Cursor" (TI-30Xa-style,
// immediate execution) and a "Cursor" (TI-30XIIS-style, expression entry) mode,
// and Class II can switch its display between SuperMathFont v2.1 pretty math and
// plain one-line text. The evaluation logic lives in CalcLogic (superwin_core).
#include <Windows.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

#include "app/Ui.h"
#include "core/Strings.h"
#include "modules/calc/CalcLogic.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kE = 2.71828182845904523536;

// Narrow an ASCII-only wide string (used for function names like L"sin").
std::string NarrowAscii(const std::wstring& w) {
    std::string s;
    s.reserve(w.size());
    for (wchar_t c : w) s += static_cast<char>(c & 0x7F);
    return s;
}

// Beautify a plain infix string for the SuperMathFont display: ·, ÷, √, superscripts.
std::wstring Beautify(const std::wstring& in) {
    std::wstring out;
    for (size_t i = 0; i < in.size(); ++i) {
        const wchar_t c = in[i];
        if (c == L'*') { out += L'·'; continue; }                 // ·
        if (c == L'^' && i + 1 < in.size()) {
            const wchar_t n = in[i + 1];
            const wchar_t* sup = L"⁰¹²³⁴⁵⁶⁷⁸⁹";
            if (n >= L'0' && n <= L'9') { out += sup[n - L'0']; ++i; continue; }
        }
        out += c;
    }
    // word replacements
    auto replaceAll = [](std::wstring& s, const std::wstring& a, const std::wstring& b) {
        for (size_t p = s.find(a); p != std::wstring::npos; p = s.find(a, p + b.size()))
            s.replace(p, a.size(), b);
    };
    replaceAll(out, L"sqrt(", L"√(");  // √(
    replaceAll(out, L"pi", L"π");      // π
    return out;
}

struct CalcSpec {
    int classNum = 1;
    bool scientific = false;   // trig / log / powers (Class II+)
    bool advanced = false;     // inverse trig, hyperbolics, more (Class III)
    bool hasModes = false;     // Cursor / Non-Cursor selector (Class I & II)
    bool fontToggle = false;   // SuperMathFont v2.1 toggle (Class II)
};

enum class KKind { Digit, Dot, Op, Pow, Func, Const, Paren, Equals, Clear, Back, Percent, Sqr, Fact, Negate, Empty };
struct Key {
    std::wstring label;
    KKind kind = KKind::Empty;
    std::wstring payload;
    int span = 1;
    bool accent = false;
};

// One calculator (Class I-III). Class IV embeds the graphing page instead.
class CalcPanel {
public:
    explicit CalcPanel(CalcSpec spec) : spec_(spec) { Build(); }
    winrt::UIElement Root() { return root_; }

private:
    enum class Mode { Cursor, Immediate };

    void Build() {
        auto col = ui::VStack(12);

        // --- options row (modes, angle, font) ---
        auto opts = ui::HStack(14);
        opts.VerticalAlignment(winrt::VerticalAlignment::Center);
        if (spec_.hasModes) {
            modeBox_ = winrt::ComboBox();
            modeBox_.Items().Append(winrt::box_value(winrt::hstring(L"Cursor")));
            modeBox_.Items().Append(winrt::box_value(winrt::hstring(L"Non-Cursor")));
            modeBox_.SelectedIndex(0);
            modeBox_.SelectionChanged([this](winrt::IInspectable const&, winrt::SelectionChangedEventArgs const&) {
                mode_ = modeBox_.SelectedIndex() == 1 ? Mode::Immediate : Mode::Cursor;
                expr_.clear(); imm_.ClearAll(); justEvaluated_ = false; Refresh();
            });
            auto l = ui::Caption(L"Mode"); l.VerticalAlignment(winrt::VerticalAlignment::Center);
            opts.Children().Append(l);
            opts.Children().Append(modeBox_);
        }
        if (spec_.scientific) {
            angleBox_ = winrt::ComboBox();
            angleBox_.Items().Append(winrt::box_value(winrt::hstring(L"RAD")));
            angleBox_.Items().Append(winrt::box_value(winrt::hstring(L"DEG")));
            angleBox_.SelectedIndex(0);
            angleBox_.SelectionChanged([this](winrt::IInspectable const&, winrt::SelectionChangedEventArgs const&) {
                angle_ = angleBox_.SelectedIndex() == 1 ? AngleMode::Degrees : AngleMode::Radians;
                Refresh();
            });
            auto l = ui::Caption(L"Angle"); l.VerticalAlignment(winrt::VerticalAlignment::Center);
            opts.Children().Append(l);
            opts.Children().Append(angleBox_);
        }
        if (spec_.fontToggle) {
            fontCheck_ = winrt::CheckBox();
            fontCheck_.Content(winrt::box_value(winrt::hstring(L"SuperMathFont v2.1")));
            fontCheck_.IsChecked(false);
            auto onToggle = [this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
                prettyFont_ = fontCheck_.IsChecked().Value();
                ApplyFonts(); Refresh();
            };
            fontCheck_.Checked(onToggle);
            fontCheck_.Unchecked(onToggle);
            opts.Children().Append(fontCheck_);
        }
        if (opts.Children().Size() > 0) col.Children().Append(opts);

        // --- display ---
        exprText_ = ui::Text(L"", 16);
        exprText_.HorizontalAlignment(winrt::HorizontalAlignment::Right);
        exprText_.TextAlignment(winrt::TextAlignment::Right);
        if (auto b = ui::ThemeBrush(L"TextFillColorSecondaryBrush")) exprText_.Foreground(b);
        mainText_ = ui::Text(L"0", 34, true);
        mainText_.HorizontalAlignment(winrt::HorizontalAlignment::Right);
        mainText_.TextAlignment(winrt::TextAlignment::Right);
        auto disp = ui::VStack(2);
        disp.Children().Append(exprText_);
        disp.Children().Append(mainText_);
        auto dispCard = ui::Card(disp, 18);
        dispCard.MinHeight(96);
        col.Children().Append(dispCard);
        ApplyFonts();

        // --- keypads ---
        if (spec_.scientific) col.Children().Append(GridFromKeys(FunctionKeys()));
        col.Children().Append(GridFromKeys(PadKeys()));

        auto page = ui::VStack(12);
        page.MaxWidth(560);
        page.HorizontalAlignment(winrt::HorizontalAlignment::Left);
        page.Children().Append(col);
        root_ = page;
        Refresh();
    }

    void ApplyFonts() {
        const wchar_t* fam = prettyFont_ ? L"Cambria Math" : L"Consolas";
        mainText_.FontFamily(winrt::Microsoft::UI::Xaml::Media::FontFamily(fam));
        exprText_.FontFamily(winrt::Microsoft::UI::Xaml::Media::FontFamily(fam));
    }

    // ---- key tables -------------------------------------------------------
    std::vector<std::vector<Key>> FunctionKeys() {
        std::vector<std::vector<Key>> rows;
        rows.push_back({{L"sin", KKind::Func, L"sin"}, {L"cos", KKind::Func, L"cos"}, {L"tan", KKind::Func, L"tan"}, {L"π", KKind::Const, L"pi"}});
        rows.push_back({{L"ln", KKind::Func, L"ln"}, {L"log", KKind::Func, L"log"}, {L"√", KKind::Func, L"sqrt"}, {L"e", KKind::Const, L"e"}});
        rows.push_back({{L"x²", KKind::Sqr, L""}, {L"xⁿ", KKind::Pow, L"^"}, {L"!", KKind::Fact, L""}, {L"%", KKind::Percent, L""}});
        if (spec_.advanced) {
            rows.push_back({{L"asin", KKind::Func, L"asin"}, {L"acos", KKind::Func, L"acos"}, {L"atan", KKind::Func, L"atan"}, {L"eˣ", KKind::Func, L"exp"}});
            rows.push_back({{L"sinh", KKind::Func, L"sinh"}, {L"cosh", KKind::Func, L"cosh"}, {L"tanh", KKind::Func, L"tanh"}, {L"log₂", KKind::Func, L"log2"}});
            rows.push_back({{L"∛", KKind::Func, L"cbrt"}, {L"abs", KKind::Func, L"abs"}, {L"(", KKind::Paren, L"("}, {L")", KKind::Paren, L")"}});
        } else {
            rows.push_back({{L"(", KKind::Paren, L"("}, {L")", KKind::Paren, L")"}, {L"eˣ", KKind::Func, L"exp"}, {L"±", KKind::Negate, L""}});
        }
        return rows;
    }

    std::vector<std::vector<Key>> PadKeys() {
        std::vector<std::vector<Key>> rows;
        if (!spec_.scientific) {
            // Class I gets parens/%/√ here since it has no function area.
            rows.push_back({{L"%", KKind::Percent, L""}, {L"√", KKind::Func, L"sqrt"}, {L"(", KKind::Paren, L"("}, {L")", KKind::Paren, L")"}});
        }
        rows.push_back({{L"C", KKind::Clear, L"", 1, false}, {L"CE", KKind::Back, L""}, {L"⌫", KKind::Back, L""}, {L"÷", KKind::Op, L"/"}});
        rows.push_back({{L"7", KKind::Digit, L"7"}, {L"8", KKind::Digit, L"8"}, {L"9", KKind::Digit, L"9"}, {L"×", KKind::Op, L"*"}});
        rows.push_back({{L"4", KKind::Digit, L"4"}, {L"5", KKind::Digit, L"5"}, {L"6", KKind::Digit, L"6"}, {L"−", KKind::Op, L"-"}});
        rows.push_back({{L"1", KKind::Digit, L"1"}, {L"2", KKind::Digit, L"2"}, {L"3", KKind::Digit, L"3"}, {L"+", KKind::Op, L"+"}});
        rows.push_back({{L"±", KKind::Negate, L""}, {L"0", KKind::Digit, L"0"}, {L".", KKind::Dot, L"."}, {L"=", KKind::Equals, L"", 1, true}});
        return rows;
    }

    winrt::Grid GridFromKeys(const std::vector<std::vector<Key>>& rows) {
        winrt::Grid g;
        g.ColumnSpacing(0);
        g.RowSpacing(0);
        size_t cols = 0;
        for (auto const& r : rows) cols = std::max(cols, r.size());
        for (size_t c = 0; c < cols; ++c) {
            auto cd = winrt::ColumnDefinition();
            cd.Width(winrt::GridLengthHelper::FromValueAndType(1, winrt::GridUnitType::Star));
            g.ColumnDefinitions().Append(cd);
        }
        for (size_t r = 0; r < rows.size(); ++r) {
            auto rd = winrt::RowDefinition();
            rd.Height(winrt::GridLengthHelper::Auto());
            g.RowDefinitions().Append(rd);
        }
        for (size_t r = 0; r < rows.size(); ++r) {
            int c = 0;
            for (auto const& k : rows[r]) {
                if (k.kind != KKind::Empty) {
                    auto btn = MakeKey(k);
                    winrt::Grid::SetRow(btn, static_cast<int>(r));
                    winrt::Grid::SetColumn(btn, c);
                    if (k.span > 1) winrt::Grid::SetColumnSpan(btn, k.span);
                    g.Children().Append(btn);
                }
                c += k.span;
            }
        }
        return g;
    }

    winrt::Button MakeKey(Key k) {
        winrt::Button btn;
        btn.Content(winrt::box_value(k.label));
        btn.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);
        btn.VerticalAlignment(winrt::VerticalAlignment::Stretch);
        btn.MinHeight(48);
        btn.Margin(winrt::Thickness{3, 3, 3, 3});
        btn.CornerRadius(winrt::CornerRadius{10, 10, 10, 10});
        btn.FontSize(16);
        if (k.accent) {
            if (auto st = winrt::Application::Current().Resources()
                              .TryLookup(winrt::box_value(winrt::hstring(L"AccentButtonStyle")))
                              .try_as<winrt::Style>())
                btn.Style(st);
        }
        btn.Click([this, k](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { Dispatch(k); });
        return btn;
    }

    // ---- input dispatch ---------------------------------------------------
    bool immediate() const { return mode_ == Mode::Immediate; }

    void Append(const std::wstring& s, bool startsNew) {
        if (startsNew && justEvaluated_) expr_.clear();
        justEvaluated_ = false;
        expr_ += s;
    }

    void Dispatch(const Key& k) {
        switch (k.kind) {
            case KKind::Digit:
                if (immediate()) imm_.Digit(k.payload[0] - L'0');
                else Append(k.payload, true);
                break;
            case KKind::Dot:
                if (immediate()) imm_.Dot();
                else Append(L".", false);
                break;
            case KKind::Op:
                if (immediate()) imm_.Op(static_cast<char>(k.payload[0]));
                else Append(k.payload, false);
                break;
            case KKind::Pow:
                if (immediate()) imm_.Op('^');
                else Append(L"^", false);
                break;
            case KKind::Func:
                if (immediate()) imm_.Func(NarrowAscii(k.payload), angle_);
                else Append(k.payload + L"(", true);
                break;
            case KKind::Const:
                if (immediate()) imm_.SetConst(k.payload == L"pi" ? kPi : kE);
                else Append(k.payload, true);
                break;
            case KKind::Paren:
                if (!immediate()) Append(k.payload, k.payload == L"(");
                break;
            case KKind::Sqr:
                if (immediate()) imm_.Func("sqr", angle_);
                else Append(L"^2", false);
                break;
            case KKind::Fact:
                if (immediate()) imm_.Func("fact", angle_);
                else Append(L"!", false);
                break;
            case KKind::Percent:
                if (immediate()) imm_.Percent();
                else Append(L"%", false);
                break;
            case KKind::Negate:
                if (immediate()) imm_.Negate();
                else NegateCursor();
                break;
            case KKind::Back:
                if (immediate()) imm_.Backspace();
                else if (!expr_.empty()) { expr_.pop_back(); justEvaluated_ = false; }
                break;
            case KKind::Clear:
                if (immediate()) imm_.ClearAll();
                else { expr_.clear(); justEvaluated_ = false; }
                break;
            case KKind::Equals:
                DoEquals();
                break;
            case KKind::Empty:
                break;
        }
        Refresh();
    }

    void NegateCursor() {
        if (expr_.empty()) { expr_ = L"-"; return; }
        std::string err;
        auto v = EvaluateCalc(NarrowUtf8(expr_), angle_, err);
        if (v) { expr_ = Widen(FormatCalc(-*v)); justEvaluated_ = true; }
    }

    void DoEquals() {
        if (immediate()) { imm_.Equals(); return; }
        std::string err;
        auto v = EvaluateCalc(NarrowUtf8(expr_), angle_, err);
        if (v) { expr_ = Widen(FormatCalc(*v)); justEvaluated_ = true; }
        else if (!expr_.empty()) { errorFlash_ = true; }
    }

    void Refresh() {
        if (immediate()) {
            exprText_.Text(L"");
            mainText_.Text(winrt::hstring(Widen(imm_.Display())));
            return;
        }
        exprText_.Text(winrt::hstring(prettyFont_ ? Beautify(expr_) : expr_));
        if (errorFlash_) { mainText_.Text(L"Error"); errorFlash_ = false; return; }
        if (expr_.empty()) { mainText_.Text(L"0"); return; }
        std::string err;
        auto v = EvaluateCalc(NarrowUtf8(expr_), angle_, err);
        mainText_.Text(winrt::hstring(v ? Widen(FormatCalc(*v)) : std::wstring(L"")));
    }

    static std::string NarrowUtf8(const std::wstring& w) { return WideToUtf8(w); }
    static std::wstring Widen(const std::string& s) { return Utf8ToWide(s); }

    CalcSpec spec_;
    Mode mode_ = Mode::Cursor;
    AngleMode angle_ = AngleMode::Radians;
    bool prettyFont_ = false;
    bool justEvaluated_ = false;
    bool errorFlash_ = false;
    std::wstring expr_;
    ImmediateCalc imm_;

    winrt::UIElement root_{nullptr};
    winrt::TextBlock exprText_{nullptr};
    winrt::TextBlock mainText_{nullptr};
    winrt::ComboBox modeBox_{nullptr};
    winrt::ComboBox angleBox_{nullptr};
    winrt::CheckBox fontCheck_{nullptr};
};

// The page: a TabView of the four classes.
class CalcPage : public IModulePage {
public:
    CalcPage() { Build(); }
    winrt::UIElement Root() override { return root_; }
    void OnShown() override { if (graph_) graph_->OnShown(); }
    void OnHidden() override { if (graph_) graph_->OnHidden(); }

private:
    void Build() {
        tabs_ = winrt::TabView();
        tabs_.IsAddTabButtonVisible(false);
        tabs_.TabWidthMode(winrt::TabViewWidthMode::Equal);
        tabs_.CanReorderTabs(false);
        tabs_.CanDragTabs(false);

        AddTab(L"Class I", CalcSpec{1, false, false, true, false});
        AddTab(L"Class II", CalcSpec{2, true, false, true, true});
        AddTab(L"Class III", CalcSpec{3, true, true, false, false});

        // Class IV CAS: the graphing/CAS engine.
        graph_ = MakeGraphPage();
        auto t4 = winrt::TabViewItem();
        t4.Header(winrt::box_value(winrt::hstring(L"Class IV CAS")));
        t4.IsClosable(false);
        t4.Content(graph_->Root());
        tabs_.TabItems().Append(t4);

        tabs_.SelectedIndex(0);
        root_ = ui::Page(L"Calculator", tabs_);
    }

    void AddTab(const wchar_t* header, CalcSpec spec) {
        auto panel = std::make_unique<CalcPanel>(spec);
        auto t = winrt::TabViewItem();
        t.Header(winrt::box_value(winrt::hstring(header)));
        t.IsClosable(false);
        t.Content(panel->Root());
        tabs_.TabItems().Append(t);
        panels_.push_back(std::move(panel));
    }

    winrt::UIElement root_{nullptr};
    winrt::TabView tabs_{nullptr};
    std::vector<std::unique_ptr<CalcPanel>> panels_;
    std::unique_ptr<IModulePage> graph_;
};

}  // namespace

std::unique_ptr<IModulePage> MakeCalcPage() {
    return std::make_unique<CalcPage>();
}

}  // namespace superwin
