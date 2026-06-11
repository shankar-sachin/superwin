// Calculator module: one page with four tabs mirroring the real TI calculator
// tiers --
//   * Class I   "4-function"  : basic arithmetic (TI-108).
//   * Class II  "scientific"  : trig + inverse trig + hyperbolics, logs, powers,
//                               roots, factorial (TI-30XS / TI-36X Pro tier).
//   * Class III "graphing"    : the WebView2/MathLive + canvas plotter.
//   * Class IV  "CAS"         : symbolic algebra (simplify, d/dx, ∫, solve).
// Class I & II share the sleek keypad; a feature spec decides which keys and modes
// each exposes, and both offer a "Non-Cursor" (TI-30Xa-style immediate AOS
// execution, with a true CE and the pending-op echo up top) and a "Cursor"
// (TI-30XIIS-style EOS expression entry, with an Ans key and a blinking block
// caret) mode. Both are skinned as a physical device: graphite body, pale-green
// LCD, and TI-style role-coloured keys (dark digits, light functions, gold
// constants, pink operators, red C) -- see the palette near the top. Class II
// carries a TI-style one-shot [2nd] modifier (sin->sin⁻¹, ln->eˣ, log->10ˣ,
// √->∛, x²->x³, !->%, π->e, abs->x⁻¹, ...) and can switch its display between
// SuperMathFont v2.1 pretty math (true ³⁄₄ fractions, superscript exponents)
// and plain text.
// Class III embeds MakeGraphPage(); Class IV is a native panel driving the CAS
// (Expr/Cas). The numeric logic lives in CalcLogic + Expr + Cas (superwin_core).
#include <Windows.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Documents.h>
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

// Every keypad class shares one device footprint so switching tabs doesn't resize
// the calculator: a fixed width and a fixed-height keypad whose rows star-stretch.
constexpr double kDeviceWidth = 384;
constexpr double kKeypadHeight = 392;

// ---- the device skin -------------------------------------------------------
// Explicit colours (not theme brushes) so Class I & II look like a real object
// in both app themes: a graphite TI-30Xa-style body with a pale-green LCD, dark
// digit keys, light function keys, gold constants and a pink TI-30XIIS-style
// operator column.
inline winrt::Windows::UI::Color Rgb(uint8_t r, uint8_t g, uint8_t b) {
    return winrt::Windows::UI::Color{0xFF, r, g, b};
}
const winrt::Windows::UI::Color kBodyBg   = Rgb(0x33, 0x36, 0x3C);  // graphite body
const winrt::Windows::UI::Color kBodyEdge = Rgb(0x1C, 0x1E, 0x22);
const winrt::Windows::UI::Color kLcdBg    = Rgb(0xC9, 0xD6, 0xBC);  // pale-green LCD
const winrt::Windows::UI::Color kLcdInk   = Rgb(0x17, 0x1D, 0x14);  // LCD segments
const winrt::Windows::UI::Color kLcdDim   = Rgb(0x44, 0x52, 0x40);  // echo/annunciator
const winrt::Windows::UI::Color kKeyDigit = Rgb(0x24, 0x26, 0x2A);  // near-black digits
const winrt::Windows::UI::Color kKeyFn    = Rgb(0xDB, 0xDD, 0xE0);  // light-grey functions
const winrt::Windows::UI::Color kKeyConst = Rgb(0xE6, 0xD5, 0x9A);  // gold π / e
const winrt::Windows::UI::Color kKeyOp    = Rgb(0xD7, 0x4F, 0x9F);  // pink operators
const winrt::Windows::UI::Color kKeyEq    = Rgb(0xF0, 0x72, 0xBC);  // brighter pink =
const winrt::Windows::UI::Color kKeyClear = Rgb(0xC2, 0x44, 0x35);  // red C
const winrt::Windows::UI::Color kKeyWarm  = Rgb(0xDE, 0x82, 0x26);  // orange ⌫ / Ans
const winrt::Windows::UI::Color kInkLight = Rgb(0xFF, 0xFF, 0xFF);
const winrt::Windows::UI::Color kInkDark  = Rgb(0x1E, 0x1F, 0x22);

winrt::Windows::UI::Color Mix(winrt::Windows::UI::Color c, winrt::Windows::UI::Color to, double t) {
    auto ch = [t](uint8_t a, uint8_t b) {
        return static_cast<uint8_t>(a + (static_cast<int>(b) - a) * t + 0.5);
    };
    return winrt::Windows::UI::Color{0xFF, ch(c.R, to.R), ch(c.G, to.G), ch(c.B, to.B)};
}

winrt::Microsoft::UI::Xaml::Media::SolidColorBrush Solid(winrt::Windows::UI::Color c) {
    return winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(c);
}

// Narrow an ASCII-only wide string (used for function names like L"sin").
std::string NarrowAscii(const std::wstring& w) {
    std::string s;
    s.reserve(w.size());
    for (wchar_t c : w) s += static_cast<char>(c & 0x7F);
    return s;
}

constexpr const wchar_t* kSupDigits = L"⁰¹²³⁴⁵⁶⁷⁸⁹";
constexpr const wchar_t* kSubDigits = L"₀₁₂₃₄₅₆₇₈₉";

// SuperMathFont fractions: render integer/integer as a true fraction with a
// raised numerator and lowered denominator (3/4 -> ³⁄₄, U+2044 fraction slash).
// Only plain-integer operands qualify -- a neighbouring '.', '^', '!' or letter
// means the '/' is not a simple numeric fraction (1/2^3, 3.5/2) and the display
// must not regroup it.
std::wstring BeautifyFractions(const std::wstring& in) {
    auto digit = [](wchar_t c) { return c >= L'0' && c <= L'9'; };
    auto inNumber = [&](wchar_t c) {
        return digit(c) || c == L'.' || c == L'^' || c == L'!' ||
               (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
    };
    std::wstring out;
    size_t i = 0;
    while (i < in.size()) {
        if (digit(in[i]) && (out.empty() || !inNumber(out.back()))) {
            size_t j = i;
            while (j < in.size() && digit(in[j])) ++j;
            if (j < in.size() && in[j] == L'/' && j + 1 < in.size() && digit(in[j + 1])) {
                size_t k = j + 1;
                while (k < in.size() && digit(in[k])) ++k;
                const wchar_t after = k < in.size() ? in[k] : L'\0';
                if (after != L'.' && after != L'^' && after != L'!') {
                    for (size_t p = i; p < j; ++p) out += kSupDigits[in[p] - L'0'];
                    out += L'⁄';  // U+2044
                    for (size_t p = j + 1; p < k; ++p) out += kSubDigits[in[p] - L'0'];
                    i = k;
                    continue;
                }
            }
            out.append(in, i, j - i);
            i = j;
            continue;
        }
        out += in[i++];
    }
    return out;
}

// Beautify a plain infix string for the SuperMathFont display: ·, √, π, true
// numeric fractions (³⁄₄) and superscript exponents (x², 5⁻¹, 2³⁴).
std::wstring Beautify(const std::wstring& in) {
    const std::wstring frac = BeautifyFractions(in);
    std::wstring out;
    for (size_t i = 0; i < frac.size(); ++i) {
        const wchar_t c = frac[i];
        if (c == L'*') { out += L'·'; continue; }                 // ·
        if (c == L'^' && i + 1 < frac.size()) {
            size_t j = i + 1;
            std::wstring sup;
            bool neg = false;
            if (frac[j] == L'-') { neg = true; ++j; }
            while (j < frac.size() && frac[j] >= L'0' && frac[j] <= L'9') {
                sup += kSupDigits[frac[j] - L'0'];
                ++j;
            }
            // Whole-number exponents only: ^3.5 keeps the caret so the display
            // can't read as 2³·5.
            if (!sup.empty() && (j >= frac.size() || frac[j] != L'.')) {
                if (neg) out += L'⁻';
                out += sup;
                i = j - 1;
                continue;
            }
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

enum class KKind { Digit, Dot, Op, Pow, Func, Const, Paren, Equals, Clear, ClearEntry, Back, Percent, Sqr, Fact, Negate, Ans,
                   Second, Cube, Inv, Exp10, Exp2, Empty };
struct Key {
    std::wstring label;
    KKind kind = KKind::Empty;
    std::wstring payload;
    int span = 1;
    bool accent = false;
    // The TI-style shifted function this key carries while [2nd] is latched
    // (kind2 == Empty means the key has no second function).
    std::wstring label2;
    KKind kind2 = KKind::Empty;
    std::wstring payload2;
};

// One keypad calculator (Class I basic or Class II scientific). Class III embeds
// the graphing page and Class IV the CAS panel instead.
class CalcPanel {
public:
    explicit CalcPanel(CalcSpec spec) : spec_(spec) { Build(); }
    winrt::UIElement Root() { return root_; }

    // Pause the caret blink while the page is hidden (no work when not visible).
    void OnShown() { if (caretTimer_) caretTimer_.Start(); }
    void OnHidden() { if (caretTimer_) caretTimer_.Stop(); }

private:
    enum class Mode { Cursor, Immediate };

    void Build() {
        // --- options row (modes, angle, font) ---
        auto opts = ui::HStack(14);
        opts.VerticalAlignment(winrt::VerticalAlignment::Center);
        if (spec_.hasModes) {
            modeBox_ = winrt::ComboBox();
            modeBox_.Items().Append(winrt::box_value(winrt::hstring(L"Cursor (TI-30XIIS)")));
            modeBox_.Items().Append(winrt::box_value(winrt::hstring(L"Non-Cursor (TI-30Xa)")));
            modeBox_.SelectedIndex(0);
            modeBox_.SelectionChanged([this](winrt::IInspectable const&, winrt::SelectionChangedEventArgs const&) {
                mode_ = modeBox_.SelectedIndex() == 1 ? Mode::Immediate : Mode::Cursor;
                expr_.clear(); imm_.ClearAll(); justEvaluated_ = false; second_ = false;
                FillKeypad();  // Cursor shows an Ans key where Non-Cursor has CE
                Refresh();
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
        // --- display: a status line (DEG/RAD annunciator left, history echo right)
        // over one big right-aligned main line, on a pale-green LCD like the real
        // TI two-line displays.
        indText_ = ui::Text(L"", 11.5);
        indText_.VerticalAlignment(winrt::VerticalAlignment::Center);
        indText_.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
        indText_.Foreground(Solid(kLcdDim));
        exprText_ = ui::Text(L"", 14);
        exprText_.HorizontalAlignment(winrt::HorizontalAlignment::Right);
        exprText_.TextAlignment(winrt::TextAlignment::Right);
        exprText_.TextWrapping(winrt::TextWrapping::NoWrap);
        exprText_.TextTrimming(winrt::TextTrimming::CharacterEllipsis);
        exprText_.VerticalAlignment(winrt::VerticalAlignment::Center);
        exprText_.Foreground(Solid(kLcdDim));
        auto statusRow = winrt::Grid();
        {
            auto c0 = winrt::ColumnDefinition();
            c0.Width(winrt::GridLengthHelper::FromValueAndType(0, winrt::GridUnitType::Auto));
            auto c1 = winrt::ColumnDefinition();
            c1.Width(winrt::GridLengthHelper::FromValueAndType(1, winrt::GridUnitType::Star));
            statusRow.ColumnDefinitions().Append(c0);
            statusRow.ColumnDefinitions().Append(c1);
        }
        winrt::Grid::SetColumn(indText_, 0);
        winrt::Grid::SetColumn(exprText_, 1);
        statusRow.Children().Append(indText_);
        statusRow.Children().Append(exprText_);
        // The main line is two Runs -- the value and a block caret -- so the caret
        // can blink (by toggling its Foreground) without the text shifting.
        mainText_ = ui::Text(L"", 40, true);
        mainText_.HorizontalAlignment(winrt::HorizontalAlignment::Right);
        mainText_.TextAlignment(winrt::TextAlignment::Right);
        mainText_.TextWrapping(winrt::TextWrapping::NoWrap);
        mainText_.TextTrimming(winrt::TextTrimming::CharacterEllipsis);
        mainText_.VerticalAlignment(winrt::VerticalAlignment::Bottom);
        mainText_.Foreground(Solid(kLcdInk));
        mainRun_ = winrt::Microsoft::UI::Xaml::Documents::Run();
        caretRun_ = winrt::Microsoft::UI::Xaml::Documents::Run();
        caretRun_.Foreground(Solid(kLcdInk));
        mainText_.Inlines().Append(mainRun_);
        mainText_.Inlines().Append(caretRun_);
        auto disp = winrt::Grid();
        {
            auto r0 = winrt::RowDefinition();
            r0.Height(winrt::GridLengthHelper::FromValueAndType(0, winrt::GridUnitType::Auto));
            auto r1 = winrt::RowDefinition();
            r1.Height(winrt::GridLengthHelper::FromValueAndType(1, winrt::GridUnitType::Star));
            disp.RowDefinitions().Append(r0);
            disp.RowDefinitions().Append(r1);
        }
        winrt::Grid::SetRow(statusRow, 0);
        winrt::Grid::SetRow(mainText_, 1);
        disp.Children().Append(statusRow);
        disp.Children().Append(mainText_);
        auto dispCard = ui::Card(disp, 16);
        dispCard.Height(112);
        dispCard.Background(Solid(kLcdBg));
        dispCard.BorderBrush(Solid(Mix(kLcdBg, kBodyEdge, 0.55)));
        dispCard.BorderThickness(winrt::Thickness{2, 2, 2, 2});
        dispCard.CornerRadius(winrt::CornerRadius{10, 10, 10, 10});
        ApplyFonts();

        // --- keypad: a fixed-height grid so every class is the SAME visual size.
        // Class I (few keys) gets big keys; Class II (many keys) gets compact keys,
        // both filling the same device body via star-sized rows. Rebuilt on mode
        // switches because the Cursor pad has an Ans key where Non-Cursor has CE.
        kpad_ = winrt::Grid();
        kpad_.Height(kKeypadHeight);
        kpad_.RowSpacing(0);
        FillKeypad();

        // The "device": display + keypad framed together at a fixed width, on a
        // graphite body so it reads as a physical calculator on the page.
        auto deviceCol = ui::VStack(10);
        deviceCol.Children().Append(dispCard);
        deviceCol.Children().Append(kpad_);
        auto device = ui::Card(deviceCol, 14);
        device.Width(kDeviceWidth);
        device.HorizontalAlignment(winrt::HorizontalAlignment::Left);
        device.Background(Solid(kBodyBg));
        device.BorderBrush(Solid(kBodyEdge));
        device.BorderThickness(winrt::Thickness{1, 1, 1, 1});
        device.CornerRadius(winrt::CornerRadius{18, 18, 18, 18});

        auto page = ui::VStack(12);
        page.HorizontalAlignment(winrt::HorizontalAlignment::Left);
        if (opts.Children().Size() > 0) page.Children().Append(opts);
        page.Children().Append(device);
        root_ = page;

        // Blinking entry caret for Cursor mode (the TI-30XIIS block cursor).
        caretTimer_ = winrt::DispatcherTimer();
        caretTimer_.Interval(std::chrono::milliseconds(530));
        caretTimer_.Tick([this](winrt::IInspectable const&, winrt::IInspectable const&) {
            caretOn_ = !caretOn_;
            caretRun_.Foreground(caretOn_ ? Solid(kLcdInk) : Solid(kLcdBg));
        });
        caretTimer_.Start();
        Refresh();
    }

    void FillKeypad() {
        kpad_.Children().Clear();
        kpad_.RowDefinitions().Clear();
        if (spec_.scientific) {
            auto fnRows = FunctionKeys();
            auto padRows = PadKeys();
            auto rdF = winrt::RowDefinition();
            rdF.Height(winrt::GridLengthHelper::FromValueAndType(static_cast<double>(fnRows.size()), winrt::GridUnitType::Star));
            auto rdP = winrt::RowDefinition();
            rdP.Height(winrt::GridLengthHelper::FromValueAndType(static_cast<double>(padRows.size()), winrt::GridUnitType::Star));
            kpad_.RowDefinitions().Append(rdF);
            kpad_.RowDefinitions().Append(rdP);
            auto fnGrid = GridFromKeys(fnRows);
            auto padGrid = GridFromKeys(padRows);
            winrt::Grid::SetRow(fnGrid, 0);
            winrt::Grid::SetRow(padGrid, 1);
            kpad_.Children().Append(fnGrid);
            kpad_.Children().Append(padGrid);
        } else {
            auto rd = winrt::RowDefinition();
            rd.Height(winrt::GridLengthHelper::FromValueAndType(1, winrt::GridUnitType::Star));
            kpad_.RowDefinitions().Append(rd);
            auto padGrid = GridFromKeys(PadKeys());
            winrt::Grid::SetRow(padGrid, 0);
            kpad_.Children().Append(padGrid);
        }
    }

    void ApplyFonts() {
        const wchar_t* fam = prettyFont_ ? L"Cambria Math" : L"Consolas";
        mainText_.FontFamily(winrt::Microsoft::UI::Xaml::Media::FontFamily(fam));
        exprText_.FontFamily(winrt::Microsoft::UI::Xaml::Media::FontFamily(fam));
    }

    // ---- key tables -------------------------------------------------------
    // Class II carries a TI-30XS/XIIS-style [2nd] modifier: most function keys
    // have a shifted second function (sin->sin⁻¹, ln->eˣ, √->∛, ...), shown by
    // rebuilding the keypad with the shifted labels while 2nd is latched.
    std::vector<std::vector<Key>> FunctionKeys() {
        std::vector<std::vector<Key>> rows;
        if (spec_.advanced) {
            rows.push_back({{L"2nd", KKind::Second, L""},
                            {L"sin", KKind::Func, L"sin", 1, false, L"sin⁻¹", KKind::Func, L"asin"},
                            {L"cos", KKind::Func, L"cos", 1, false, L"cos⁻¹", KKind::Func, L"acos"},
                            {L"tan", KKind::Func, L"tan", 1, false, L"tan⁻¹", KKind::Func, L"atan"}});
            rows.push_back({{L"sinh", KKind::Func, L"sinh", 1, false, L"sinh⁻¹", KKind::Func, L"asinh"},
                            {L"cosh", KKind::Func, L"cosh", 1, false, L"cosh⁻¹", KKind::Func, L"acosh"},
                            {L"tanh", KKind::Func, L"tanh", 1, false, L"tanh⁻¹", KKind::Func, L"atanh"},
                            {L"π", KKind::Const, L"pi", 1, false, L"e", KKind::Const, L"e"}});
            rows.push_back({{L"ln", KKind::Func, L"ln", 1, false, L"eˣ", KKind::Func, L"exp"},
                            {L"log", KKind::Func, L"log", 1, false, L"10ˣ", KKind::Exp10, L""},
                            {L"log₂", KKind::Func, L"log2", 1, false, L"2ˣ", KKind::Exp2, L""},
                            {L"abs", KKind::Func, L"abs", 1, false, L"x⁻¹", KKind::Inv, L""}});
            rows.push_back({{L"x²", KKind::Sqr, L"", 1, false, L"x³", KKind::Cube, L""},
                            {L"√", KKind::Func, L"sqrt", 1, false, L"∛", KKind::Func, L"cbrt"},
                            {L"xⁿ", KKind::Pow, L"^"},
                            {L"!", KKind::Fact, L"", 1, false, L"%", KKind::Percent, L""}});
            rows.push_back({{L"(", KKind::Paren, L"(", 2}, {L")", KKind::Paren, L")", 2}});
        } else {
            rows.push_back({{L"sin", KKind::Func, L"sin"}, {L"cos", KKind::Func, L"cos"}, {L"tan", KKind::Func, L"tan"}, {L"π", KKind::Const, L"pi"}});
            rows.push_back({{L"ln", KKind::Func, L"ln"}, {L"log", KKind::Func, L"log"}, {L"√", KKind::Func, L"sqrt"}, {L"e", KKind::Const, L"e"}});
            rows.push_back({{L"x²", KKind::Sqr, L""}, {L"xⁿ", KKind::Pow, L"^"}, {L"!", KKind::Fact, L""}, {L"%", KKind::Percent, L""}});
            rows.push_back({{L"(", KKind::Paren, L"("}, {L")", KKind::Paren, L")"}, {L"eˣ", KKind::Func, L"exp"}, {L"±", KKind::Negate, L""}});
        }
        return rows;
    }

    std::vector<std::vector<Key>> PadKeys() {
        std::vector<std::vector<Key>> rows;
        if (!spec_.scientific) {
            // Class I is a true 4-function: percent and square root only -- no
            // parentheses or exponent, like a TI-108. Two wide keys fill the row.
            rows.push_back({{L"%", KKind::Percent, L"", 2}, {L"√", KKind::Func, L"sqrt", 2}});
        }
        // Cursor mode gets the TI-30XIIS Ans key (recall the previous result into
        // the expression); Non-Cursor gets a true CE (clear the current entry,
        // keep the pending AOS chain).
        rows.push_back({{L"C", KKind::Clear, L"", 1, false},
                        immediate() ? Key{L"CE", KKind::ClearEntry, L""} : Key{L"Ans", KKind::Ans, L""},
                        {L"⌫", KKind::Back, L""},
                        {L"÷", KKind::Op, L"/"}});
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
        g.VerticalAlignment(winrt::VerticalAlignment::Stretch);
        size_t cols = 0;
        for (auto const& r : rows) cols = std::max(cols, r.size());
        for (size_t c = 0; c < cols; ++c) {
            auto cd = winrt::ColumnDefinition();
            cd.Width(winrt::GridLengthHelper::FromValueAndType(1, winrt::GridUnitType::Star));
            g.ColumnDefinitions().Append(cd);
        }
        // Star rows so the keys fill the fixed-height keypad area uniformly.
        for (size_t r = 0; r < rows.size(); ++r) {
            auto rd = winrt::RowDefinition();
            rd.Height(winrt::GridLengthHelper::FromValueAndType(1, winrt::GridUnitType::Star));
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
        // While [2nd] is latched, a key with a second function becomes that key.
        if (second_ && k.kind2 != KKind::Empty) {
            k.label = k.label2;
            k.kind = k.kind2;
            k.payload = k.payload2;
        }
        winrt::Button btn;
        btn.Content(winrt::box_value(k.label));
        btn.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);
        btn.VerticalAlignment(winrt::VerticalAlignment::Stretch);
        btn.MinHeight(0);  // star rows size the keys; no floor so Class II stays compact
        btn.Padding(winrt::Thickness{0, 0, 0, 0});
        btn.Margin(winrt::Thickness{3, 3, 3, 3});
        btn.CornerRadius(winrt::CornerRadius{12, 12, 12, 12});
        btn.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());

        // Role palette (TI-style): near-black digits, light-grey functions, gold
        // constants, pink operators, a brighter-pink =, a red C.
        winrt::Windows::UI::Color bg = kKeyFn, fg = kInkDark;
        double fs = 15;
        switch (k.kind) {
            case KKind::Digit:
            case KKind::Dot:
            case KKind::Negate:
                bg = kKeyDigit; fg = kInkLight; fs = 18;
                break;
            case KKind::Op:
                bg = kKeyOp; fg = kInkLight; fs = 19;
                break;
            case KKind::Equals:
                bg = kKeyEq; fg = kInkLight; fs = 20;
                break;
            case KKind::Clear:
                bg = kKeyClear; fg = kInkLight;
                break;
            case KKind::Back:
            case KKind::Ans:
                bg = kKeyWarm; fg = kInkLight;
                break;
            case KKind::Const:
                bg = kKeyConst; fs = spec_.scientific ? 14 : 15;
                break;
            case KKind::Func:
            case KKind::Pow:  // xⁿ lives in the function zone, so it dresses like one
            case KKind::Sqr:
            case KKind::Cube:
            case KKind::Inv:
            case KKind::Exp10:
            case KKind::Exp2:
            case KKind::Fact:
            case KKind::Paren:
            case KKind::Percent:
                fs = spec_.scientific ? 13.5 : 15;
                break;
            case KKind::Second:  // gold modifier key; lit orange while latched
                bg = second_ ? kKeyWarm : kKeyConst;
                fg = second_ ? kInkLight : kInkDark;
                fs = 14;
                break;
            default:
                break;  // CE keeps the light function-key look
        }
        btn.FontSize(fs);

        // Override the button's own theme resources (rest/hover/pressed) so the
        // key keeps its colour through pointer states instead of snapping back to
        // the theme grey.
        auto res = btn.Resources();
        auto put = [&res](const wchar_t* key, winrt::Windows::UI::Color c) {
            res.Insert(winrt::box_value(winrt::hstring(key)), Solid(c));
        };
        const bool dark = (k.kind == KKind::Digit || k.kind == KKind::Dot ||
                           k.kind == KKind::Negate || k.kind == KKind::Op ||
                           k.kind == KKind::Equals || k.kind == KKind::Clear ||
                           k.kind == KKind::Back || k.kind == KKind::Ans);
        const auto white = Rgb(0xFF, 0xFF, 0xFF), black = Rgb(0x00, 0x00, 0x00);
        put(L"ButtonBackground", bg);
        put(L"ButtonBackgroundPointerOver", Mix(bg, white, dark ? 0.14 : 0.45));
        put(L"ButtonBackgroundPressed", Mix(bg, black, 0.18));
        put(L"ButtonForeground", fg);
        put(L"ButtonForegroundPointerOver", fg);
        put(L"ButtonForegroundPressed", Mix(fg, bg, 0.25));
        put(L"ButtonBorderBrush", Mix(bg, black, 0.30));
        put(L"ButtonBorderBrushPointerOver", Mix(bg, black, 0.30));
        put(L"ButtonBorderBrushPressed", Mix(bg, black, 0.45));

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
            case KKind::Cube:
                if (immediate()) imm_.Func("cube", angle_);
                else Append(L"^3", false);
                break;
            case KKind::Inv:
                if (immediate()) imm_.Func("inv", angle_);
                else Append(L"^-1", false);
                break;
            case KKind::Exp10:
                if (immediate()) imm_.Func("exp10", angle_);
                else Append(L"10^(", true);
                break;
            case KKind::Exp2:
                if (immediate()) imm_.Func("exp2", angle_);
                else Append(L"2^(", true);
                break;
            case KKind::Second:
                second_ = !second_;
                FillKeypad();
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
            case KKind::ClearEntry:
                if (immediate()) imm_.ClearEntry();
                else { expr_.clear(); justEvaluated_ = false; }
                break;
            case KKind::Ans:
                if (immediate()) imm_.SetConst(lastAns_);
                else Append(L"Ans", true);
                break;
            case KKind::Equals:
                DoEquals();
                break;
            case KKind::Empty:
                break;
        }
        // [2nd] is one-shot, like the real TI: the next keypress (shifted or
        // not) releases it and the keypad returns to its primary labels.
        if (k.kind != KKind::Second && second_) {
            second_ = false;
            FillKeypad();
        }
        Refresh();
    }

    void NegateCursor() {
        if (expr_.empty()) { expr_ = L"-"; return; }
        std::string err;
        auto v = EvaluateCalc(NarrowUtf8(expr_), angle_, err, lastAns_);
        if (v) {
            evalEcho_.clear();
            lastAns_ = -*v;
            expr_ = Widen(FormatCalc(-*v));
            justEvaluated_ = true;
        }
    }

    void DoEquals() {
        if (immediate()) { imm_.Equals(); return; }
        std::string err;
        auto v = EvaluateCalc(NarrowUtf8(expr_), angle_, err, lastAns_);
        if (v) {
            evalEcho_ = (prettyFont_ ? Beautify(expr_) : expr_) + L" =";
            lastAns_ = *v;  // the Ans key recalls this
            expr_ = Widen(FormatCalc(*v));
            justEvaluated_ = true;
        } else if (!expr_.empty()) {
            errorFlash_ = true;
        }
    }

    // Set the big line. `caret` shows the blinking TI-30XIIS block cursor after
    // the text (Cursor-mode entry only); a fresh keypress always lands with the
    // caret solid so typing feels immediate.
    void SetMain(const std::wstring& text, bool caret) {
        mainRun_.Text(winrt::hstring(text));
        caretRun_.Text(caret ? L"\x258C" : L"");  // ▌
        if (caret) { caretOn_ = true; caretRun_.Foreground(Solid(kLcdInk)); }
    }

    void Refresh() {
        // Display indicators, like the real device's annunciators (DEG/RAD, and
        // 2ND while the modifier is latched).
        std::wstring ind = spec_.scientific
                               ? (angle_ == AngleMode::Degrees ? L"DEG" : L"RAD")
                               : L"";
        if (second_) ind += L"  2ND";
        indText_.Text(winrt::hstring(ind));
        // Non-Cursor (TI-30Xa): immediate execution -- a single value display that
        // clears and shows each new entry/result, with the pending operation (the
        // "previous" context, e.g. "8 ×") shown small on the line above. No caret.
        if (immediate()) {
            exprText_.Text(winrt::hstring(Widen(imm_.Echo())));
            SetMain(Widen(imm_.Display()), false);
            return;
        }
        // Cursor (TI-30XIIS / EOS): you see the WHOLE expression as you type it in
        // the main line, with a blinking block cursor at the end; pressing =
        // evaluates and lifts it to the small echo line with the result shown big
        // below -- there is no live-result preview.
        if (errorFlash_) {
            exprText_.Text(winrt::hstring(prettyFont_ ? Beautify(expr_) : expr_));
            SetMain(L"Error", false);
            errorFlash_ = false;
            return;
        }
        if (justEvaluated_) {
            exprText_.Text(winrt::hstring(evalEcho_));
            SetMain(expr_, false);  // the result of '='
            return;
        }
        exprText_.Text(L"");
        SetMain(prettyFont_ ? Beautify(expr_) : expr_, true);  // empty entry = caret alone
    }

    static std::string NarrowUtf8(const std::wstring& w) { return WideToUtf8(w); }
    static std::wstring Widen(const std::string& s) { return Utf8ToWide(s); }

    CalcSpec spec_;
    Mode mode_ = Mode::Cursor;
    AngleMode angle_ = AngleMode::Radians;
    bool second_ = false;    // [2nd] latched (Class II); released by the next key
    bool prettyFont_ = false;
    bool justEvaluated_ = false;
    bool errorFlash_ = false;
    double lastAns_ = 0.0;   // cursor mode: the value the Ans key recalls
    std::wstring expr_;
    std::wstring evalEcho_;  // cursor mode: the "<expr> =" echo shown after '='
    ImmediateCalc imm_;

    winrt::UIElement root_{nullptr};
    winrt::Grid kpad_{nullptr};
    winrt::TextBlock indText_{nullptr};
    winrt::TextBlock exprText_{nullptr};
    winrt::TextBlock mainText_{nullptr};
    winrt::Microsoft::UI::Xaml::Documents::Run mainRun_{nullptr};
    winrt::Microsoft::UI::Xaml::Documents::Run caretRun_{nullptr};
    winrt::DispatcherTimer caretTimer_{nullptr};
    bool caretOn_ = true;
    winrt::ComboBox modeBox_{nullptr};
    winrt::ComboBox angleBox_{nullptr};
    winrt::CheckBox fontCheck_{nullptr};
};

// The page: a TabView of the four classes.
class CalcPage : public IModulePage {
public:
    CalcPage() { Build(); }
    winrt::UIElement Root() override { return root_; }
    void OnShown() override {
        for (auto& p : panels_) p->OnShown();
        if (graph_) graph_->OnShown();
        if (casGraph_) casGraph_->OnShown();
    }
    void OnHidden() override {
        for (auto& p : panels_) p->OnHidden();
        if (graph_) graph_->OnHidden();
        if (casGraph_) casGraph_->OnHidden();
    }

private:
    void Build() {
        tabs_ = winrt::TabView();
        tabs_.IsAddTabButtonVisible(false);
        tabs_.TabWidthMode(winrt::TabViewWidthMode::Equal);
        tabs_.CanReorderTabs(false);
        tabs_.CanDragTabs(false);

        // Class I: 4-function basic. Class II: full scientific (advanced=true).
        AddTab(L"Class I", CalcSpec{1, false, false, true, false});
        AddTab(L"Class II", CalcSpec{2, true, true, true, true});

        // Class III: graphing calculator with no CAS (symbolic features hidden).
        graph_ = MakeGraphPage(/*cas=*/false);
        auto t3 = winrt::TabViewItem();
        t3.Header(winrt::box_value(winrt::hstring(L"Class III")));
        t3.IsClosable(false);
        t3.Content(graph_->Root());
        tabs_.TabItems().Append(t3);

        // Class IV: the full CAS graphing calculator (plot + symbolic console).
        casGraph_ = MakeGraphPage(/*cas=*/true);
        auto t4 = winrt::TabViewItem();
        t4.Header(winrt::box_value(winrt::hstring(L"Class IV CAS")));
        t4.IsClosable(false);
        t4.Content(casGraph_->Root());
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
    std::unique_ptr<IModulePage> graph_;      // Class III (graphing only)
    std::unique_ptr<IModulePage> casGraph_;   // Class IV (graphing + CAS)
};

}  // namespace

std::unique_ptr<IModulePage> MakeCalcPage() {
    return std::make_unique<CalcPage>();
}

}  // namespace superwin
