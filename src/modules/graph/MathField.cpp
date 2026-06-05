#include "modules/graph/MathField.h"

#include <algorithm>
#include <array>
#include <cwctype>

#include <winrt/Windows.Foundation.h>

#include "core/Strings.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

constexpr wchar_t kMul   = L'\x00B7';  // ·
constexpr wchar_t kMinus = L'\x2212';  // −
constexpr wchar_t kSqrt  = L'\x221A';  // √
constexpr wchar_t kPi    = L'\x03C0';  // π
constexpr wchar_t kSupMinus = L'\x207B';  // ⁻

const std::array<wchar_t, 10> kSup = {
    L'\x2070', L'\x00B9', L'\x00B2', L'\x00B3', L'\x2074',
    L'\x2075', L'\x2076', L'\x2077', L'\x2078', L'\x2079'};

bool IsSupDigit(wchar_t c) {
    return std::find(kSup.begin(), kSup.end(), c) != kSup.end();
}

// Match `word` at position i with non-letter boundaries on both sides, so we
// don't rewrite a substring inside a longer identifier.
bool MatchWord(const std::wstring& s, size_t i, const wchar_t* word) {
    size_t n = 0;
    while (word[n]) ++n;
    if (s.compare(i, n, word) != 0) return false;
    if (i > 0 && std::iswalpha(s[i - 1])) return false;
    if (i + n < s.size() && std::iswalpha(s[i + n])) return false;
    return true;
}

// Beautify `in` left-to-right into Unicode math, remapping the caret index.
std::wstring Beautify(const std::wstring& in, size_t caretIn, size_t& caretOut) {
    std::wstring out;
    out.reserve(in.size());
    bool caretSet = false;
    size_t i = 0;
    while (i < in.size()) {
        if (!caretSet && i >= caretIn) { caretOut = out.size(); caretSet = true; }
        const size_t startI = i;
        const wchar_t c = in[i];

        if (MatchWord(in, i, L"sqrt")) { out += kSqrt; i += 4; }
        else if (MatchWord(in, i, L"pi")) { out += kPi; i += 2; }
        else if (c == L'*') { out += kMul; ++i; }
        else if (c == L'-') {
            const wchar_t prev = i > 0 ? in[i - 1] : 0;  // keep ASCII '-' in 1e-3
            out += (prev == L'e' || prev == L'E') ? L'-' : kMinus;
            ++i;
        }
        else if (c == L'^') {
            size_t j = i + 1;
            bool neg = false;
            if (j < in.size() && in[j] == L'-') { neg = true; ++j; }
            if (j < in.size() && in[j] == L'(') {  // ^(...) — superscript only if all digits
                size_t k = j + 1;
                std::wstring inside;
                bool allDigits = true;
                while (k < in.size() && in[k] != L')') {
                    if (!std::iswdigit(in[k])) allDigits = false;
                    inside += in[k++];
                }
                if (k < in.size() && in[k] == L')' && allDigits && !inside.empty()) {
                    if (neg) out += kSupMinus;
                    for (wchar_t d : inside) out += kSup[d - L'0'];
                    i = k + 1;
                } else { out += L'^'; ++i; }  // leave the group literal
            } else {  // ^digits
                std::wstring digits;
                while (j < in.size() && std::iswdigit(in[j])) digits += in[j++];
                if (!digits.empty()) {
                    if (neg) out += kSupMinus;
                    for (wchar_t d : digits) out += kSup[d - L'0'];
                    i = j;
                } else { out += L'^'; ++i; }
            }
        }
        else if (std::iswdigit(c) && !out.empty() && IsSupDigit(out.back())) {
            out += kSup[c - L'0'];  // extend an exponent: 2 typed right after x²
            ++i;
        }
        else { out += c; ++i; }

        if (!caretSet && caretIn > startI && caretIn <= i) { caretOut = out.size(); caretSet = true; }
    }
    if (!caretSet) caretOut = out.size();
    return out;
}

}  // namespace

MathField::MathField() {
    box_ = winrt::TextBox();
    box_.PlaceholderText(L"f(x)   e.g.  x^2 - 3,  sin(x),  d/dx(x^3),  int(cos(x))");
    box_.FontFamily(winrt::Media::FontFamily(L"Segoe UI"));
    box_.FontSize(19);
    box_.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);
    box_.MinWidth(240);
    box_.AcceptsReturn(false);
    box_.TextChanged([this](winrt::IInspectable const&, winrt::Controls::TextChangedEventArgs const&) {
        if (guard_) return;
        guard_ = true;
        BeautifyLocked();
        guard_ = false;
        if (onChanged_) onChanged_();
    });
}

void MathField::BeautifyLocked() {
    const std::wstring cur(box_.Text());
    const size_t caretIn = static_cast<size_t>(std::max(0, box_.SelectionStart()));
    size_t caretOut = caretIn;
    const std::wstring out = Beautify(cur, caretIn, caretOut);
    if (out != cur) {
        box_.Text(winrt::hstring(out));  // re-enters TextChanged, but guard_ short-circuits it
        box_.SelectionStart(static_cast<int32_t>(std::min(caretOut, out.size())));
    }
}

std::string MathField::ToAscii() const {
    return WideToUtf8(std::wstring(box_.Text()));
}

void MathField::SetText(const std::string& s) {
    guard_ = true;
    box_.Text(winrt::hstring(Utf8ToWide(s)));
    BeautifyLocked();
    guard_ = false;
}

}  // namespace superwin
