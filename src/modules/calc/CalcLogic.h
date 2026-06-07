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

namespace superwin {

enum class AngleMode { Radians, Degrees };

// Evaluate `expr`. Returns nullopt and fills `error` on a parse/eval failure
// (including division by zero / domain errors that produce a non-finite value).
std::optional<double> EvaluateCalc(const std::string& expr, AngleMode angle, std::string& error);

// Format a value for a calculator display: trims trailing zeros, collapses -0 to
// 0, and switches to scientific notation for very large/small magnitudes.
// Returns "Error" for non-finite values.
std::string FormatCalc(double v);

// Immediate-execution calculator (no expression line). Mirrors a classic
// scientific calculator: digits build the current entry, an operator commits any
// pending operation against the running value, functions act on the shown value.
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

private:
    double display_ = 0;   // the value currently shown
    double stored_ = 0;    // left operand of a pending operation
    char   op_ = 0;        // pending operation (0 = none)
    bool   typing_ = false;// the user is entering digits into `buf_`
    bool   error_ = false;
    std::string buf_;      // digits being typed
    void Commit();         // fold a pending op into `display_`
};

}  // namespace superwin
