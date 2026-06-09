// Calculator engine for the Calculator module (Class I-IV). Pure logic in
// superwin_core (unit-tested), independent of any UI:
//   * EvaluateCalc  -- evaluate a full expression string (the "Cursor" /
//     expression-entry modes, and Class III). Supports +-*/^, parentheses,
//     unary +/-, postfix factorial (!) and percent (%), scientific functions
//     and the constants pi/e, with a selectable angle mode for trig.
//   * ImmediateCalc -- a TI-30Xa-style immediate-execution state machine (the
//     "Non-Cursor" mode): each operator applies to a running value as it is
//     pressed, with no expression line.
//   * FormatCalc    -- format a double the way a calculator display would.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace superwin {

enum class AngleMode { Radians, Degrees };

// Evaluate `expr`. Returns nullopt and fills `error` on a parse/eval failure
// (including division by zero / domain errors that produce a non-finite value).
// `ans` is the value of the `Ans` identifier (the previous result, TI-style).
std::optional<double> EvaluateCalc(const std::string& expr, AngleMode angle, std::string& error,
                                   double ans = 0.0);

// Format a value for a calculator display: trims trailing zeros, collapses -0 to
// 0, and switches to scientific notation for very large/small magnitudes.
// Returns "Error" for non-finite values.
std::string FormatCalc(double v);

// Immediate-execution calculator (no expression line). Mirrors a TI-30Xa: digits
// build the current entry, functions act instantly on the shown value, and the
// four operations plus power are resolved with full operator precedence (AOS) --
// 3 + 4 * 2 = 11, 2 ^ 3 ^ 2 = 512 -- via a stack of pending operations.
class ImmediateCalc {
public:
    void Digit(int d);                                  // 0..9
    void Dot();
    void Op(char op);                                   // '+' '-' '*' '/' '^'
    void Equals();
    void Percent();
    void Negate();                                      // +/-
    void Func(const std::string& fn, AngleMode angle);  // sin, sqrt, sqr, inv, ...
    void SetConst(double v);                            // load a constant (pi, e)
    void Backspace();
    void ClearEntry();                                  // CE: clear the current entry
    void ClearAll();                                    // C/AC: reset everything

    double value() const { return display_; }
    bool   error() const { return error_; }
    std::string Display() const;                        // formatted current display
    std::string Echo() const;                           // pending op chain, e.g. "8 × "

private:
    struct Pending { double val; char op; };  // a deferred "val op _" operation
    double display_ = 0;     // the value currently shown
    bool   typing_ = false;  // the user is entering digits into `buf_`
    bool   error_ = false;
    bool   lastWasOp_ = false;  // last key was an operator (so the next replaces it)
    std::string buf_;        // digits being typed
    std::string equalsEcho_; // after '=', the full "a op b = " echo for the top line
    std::vector<Pending> pending_;  // AOS operator stack (low precedence at bottom)
    // Fold the stack down while the top operation binds at least as tightly as
    // an incoming operator of precedence `newPrec` (right-assoc spares equal prec).
    void Reduce(int newPrec, bool rightAssoc);
};

}  // namespace superwin
