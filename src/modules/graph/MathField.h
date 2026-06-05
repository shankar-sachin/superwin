// A live "math field" for the Graphing Calculator: a TextBox that beautifies what
// you type as you type it — `*`→`·`, `sqrt`→`√`, `pi`→`π`, `-`→`−`, and integer
// exponents (`x^2`, `x^-3`) into real raised superscripts (x², x⁻³). There is no
// separate preview line: the field *is* the pretty equation.
//
// The trick that keeps this robust: the beautified Unicode text is itself directly
// re-parseable by Expr's parser (which accepts ·, ×, −, ÷, √, π and superscript
// digits), so `ToAscii()` just hands the field's text straight to the CAS — no
// fragile round-tripping between a "display" and a "source" string.
#pragma once

#include <functional>
#include <string>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

namespace superwin {

class MathField {
public:
    MathField();

    winrt::Microsoft::UI::Xaml::Controls::TextBox Control() const { return box_; }

    // The field's contents as a parser-ready UTF-8 string (the pretty Unicode is
    // accepted verbatim by ParseExpr).
    std::string ToAscii() const;

    // Replace the contents from an ASCII/Unicode expression and beautify it.
    void SetText(const std::string& s);

    // Invoked after every user edit (post-beautify) so the page can re-render.
    void OnChanged(std::function<void()> cb) { onChanged_ = std::move(cb); }

    void SetForeground(winrt::Microsoft::UI::Xaml::Media::Brush b) { box_.Foreground(b); }

private:
    void BeautifyLocked();  // transform box_ text in place, preserving the caret

    winrt::Microsoft::UI::Xaml::Controls::TextBox box_{nullptr};
    std::function<void()> onChanged_;
    bool guard_ = false;  // suppress re-entrant TextChanged from our own edits
};

}  // namespace superwin
