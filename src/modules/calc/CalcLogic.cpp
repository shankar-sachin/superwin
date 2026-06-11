#include "modules/calc/CalcLogic.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace superwin {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kE = 2.71828182845904523536;

double Factorial(double x) {
    // Integer factorial for small non-negative x; gamma for the rest.
    if (x < 0) return std::nan("");
    if (x <= 170 && std::floor(x) == x) {
        double r = 1;
        for (int i = 2; i <= static_cast<int>(x); ++i) r *= i;
        return r;
    }
    return std::tgamma(x + 1.0);
}

double ApplyUnaryFunc(const std::string& f, double x, AngleMode angle, bool& ok) {
    ok = true;
    const double toRad = angle == AngleMode::Degrees ? kPi / 180.0 : 1.0;
    const double fromRad = angle == AngleMode::Degrees ? 180.0 / kPi : 1.0;
    if (f == "sin") return std::sin(x * toRad);
    if (f == "cos") return std::cos(x * toRad);
    if (f == "tan") return std::tan(x * toRad);
    if (f == "asin") return std::asin(x) * fromRad;
    if (f == "acos") return std::acos(x) * fromRad;
    if (f == "atan") return std::atan(x) * fromRad;
    if (f == "sinh") return std::sinh(x);
    if (f == "cosh") return std::cosh(x);
    if (f == "tanh") return std::tanh(x);
    if (f == "asinh") return std::asinh(x);
    if (f == "acosh") return std::acosh(x);
    if (f == "atanh") return std::atanh(x);
    if (f == "ln") return std::log(x);
    if (f == "log" || f == "log10") return std::log10(x);
    if (f == "log2") return std::log2(x);
    if (f == "sqrt") return std::sqrt(x);
    if (f == "cbrt") return std::cbrt(x);
    if (f == "exp") return std::exp(x);
    if (f == "exp10") return std::pow(10.0, x);  // 10ˣ (2nd of log)
    if (f == "exp2") return std::exp2(x);        // 2ˣ  (2nd of log₂)
    if (f == "abs") return std::fabs(x);
    if (f == "sqr") return x * x;
    if (f == "cube") return x * x * x;
    if (f == "inv") return 1.0 / x;
    if (f == "fact") return Factorial(x);
    ok = false;
    return std::nan("");
}

// ---- recursive-descent expression evaluator --------------------------------
class Eval {
public:
    Eval(const std::string& s, AngleMode angle, double ans) : s_(s), angle_(angle), ans_(ans) {}
    double Run() {
        const double v = Expr();
        Skip();
        if (pos_ != s_.size()) throw std::runtime_error("unexpected character");
        return v;
    }

private:
    const std::string& s_;
    AngleMode angle_;
    double ans_ = 0.0;
    size_t pos_ = 0;

    void Skip() { while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_; }
    char Peek() { Skip(); return pos_ < s_.size() ? s_[pos_] : '\0'; }
    bool Eat(char c) { if (Peek() == c) { ++pos_; return true; } return false; }
    // Consume a UTF-8 byte sequence after whitespace.
    bool EatSeq(const char* seq) {
        Skip();
        const size_t n = std::strlen(seq);
        if (pos_ + n <= s_.size() && s_.compare(pos_, n, seq) == 0) { pos_ += n; return true; }
        return false;
    }
    // Peek a UTF-8 byte sequence (after whitespace) without consuming it.
    bool PeekSeq(const char* seq) {
        Skip();
        const size_t n = std::strlen(seq);
        return pos_ + n <= s_.size() && s_.compare(pos_, n, seq) == 0;
    }
    // Does the upcoming token begin a new factor, so two adjacent atoms multiply
    // implicitly (2pi, 2(3+1), 3sin(0), 2√9)? Operators do not start a factor.
    bool StartsFactor() {
        const char c = Peek();
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' ||
            std::isalpha(static_cast<unsigned char>(c)) || c == '(') return true;
        return PeekSeq("\xE2\x88\x9A") || PeekSeq("\xCF\x80");  // √  π
    }

    double Expr() {
        double v = Term();
        for (;;) {
            const char c = Peek();
            if (c == '+') { ++pos_; v += Term(); }
            else if (c == '-') { ++pos_; v -= Term(); }
            else if (EatSeq("\xE2\x88\x92")) { v -= Term(); }  // unicode minus
            else break;
        }
        return v;
    }
    double Term() {
        double v = Unary();
        for (;;) {
            const char c = Peek();
            if (c == '*') { ++pos_; v *= Unary(); }
            else if (c == '/') { ++pos_; v /= Unary(); }
            else if (EatSeq("\xC2\xB7") || EatSeq("\xC3\x97")) { v *= Unary(); }  // · ×
            else if (EatSeq("\xC3\xB7")) { v /= Unary(); }                        // ÷
            else if (StartsFactor()) { v *= Power(); }  // implicit multiplication
            else break;
        }
        return v;
    }
    // Unary +/- sits below * / but ABOVE ^, so -3^2 == -(3^2) == -9, matching the
    // graphing CAS and standard math convention.
    double Unary() {
        if (Eat('+')) return Unary();
        if (Eat('-') || EatSeq("\xE2\x88\x92")) return -Unary();
        return Power();
    }
    double Power() {
        double base = Postfix();
        if (Eat('^')) return std::pow(base, Unary());  // right-associative
        return base;
    }
    double Postfix() {
        double v = Atom();
        for (;;) {
            const char c = Peek();
            if (c == '!') { ++pos_; v = Factorial(v); }
            else if (c == '%') { ++pos_; v /= 100.0; }
            else break;
        }
        return v;
    }
    double Atom() {
        const char c = Peek();
        if (c == '(') { ++pos_; double v = Expr(); if (!Eat(')')) throw std::runtime_error("missing ')'"); return v; }
        if (EatSeq("\xCF\x80")) return kPi;                      // π
        if (EatSeq("\xE2\x88\x9A")) return std::sqrt(Atom());    // √
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') return Number();
        if (std::isalpha(static_cast<unsigned char>(c))) return Ident();
        throw std::runtime_error("unexpected character");
    }
    double Number() {
        Skip();
        size_t start = pos_;
        while (pos_ < s_.size() &&
               (std::isdigit(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '.' ||
                s_[pos_] == 'e' || s_[pos_] == 'E' ||
                ((s_[pos_] == '+' || s_[pos_] == '-') && pos_ > start &&
                 (s_[pos_ - 1] == 'e' || s_[pos_ - 1] == 'E')))) {
            ++pos_;
        }
        try { return std::stod(s_.substr(start, pos_ - start)); }
        catch (...) { throw std::runtime_error("invalid number"); }
    }
    double Ident() {
        Skip();
        size_t start = pos_;
        while (pos_ < s_.size() && std::isalpha(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        std::string id = s_.substr(start, pos_ - start);
        // Digit-suffixed function names (log2, log10, exp10, exp2): take the
        // trailing digits only when that spells a known function, so implicit
        // multiplication like "e2" or "pi3" keeps its meaning.
        size_t digEnd = pos_;
        while (digEnd < s_.size() && std::isdigit(static_cast<unsigned char>(s_[digEnd]))) ++digEnd;
        if (digEnd > pos_) {
            const std::string ext = id + s_.substr(pos_, digEnd - pos_);
            if (ext == "log2" || ext == "log10" || ext == "exp2" || ext == "exp10") {
                id = ext;
                pos_ = digEnd;
            }
        }
        if (id == "pi") return kPi;
        if (id == "e") return kE;
        if (id == "Ans" || id == "ans" || id == "ANS") return ans_;  // previous result
        if (!Eat('(')) throw std::runtime_error("unknown name '" + id + "'");
        double arg = Expr();
        if (!Eat(')')) throw std::runtime_error("missing ')'");
        bool ok = false;
        const double r = ApplyUnaryFunc(id, arg, angle_, ok);
        if (!ok) throw std::runtime_error("unknown function '" + id + "'");
        return r;
    }
};

double Compute(double a, char op, double b) {
    switch (op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/': return a / b;
        case '^': return std::pow(a, b);
        default:  return b;
    }
}

int OpPrec(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    if (op == '^') return 3;
    return 0;
}

// Display glyph for an operator in the echo line (UTF-8).
const char* OpGlyph(char op) {
    switch (op) {
        case '+': return "+";
        case '-': return "\xE2\x88\x92";  // −
        case '*': return "\xC3\x97";      // ×
        case '/': return "\xC3\xB7";      // ÷
        case '^': return "^";
        default:  return "?";
    }
}

}  // namespace

std::optional<double> EvaluateCalc(const std::string& expr, AngleMode angle, std::string& error,
                                   double ans) {
    if (expr.find_first_not_of(" \t") == std::string::npos) { error = "empty"; return std::nullopt; }
    try {
        const double v = Eval(expr, angle, ans).Run();
        if (!std::isfinite(v)) { error = "math error"; return std::nullopt; }
        return v;
    } catch (const std::exception& e) {
        error = e.what();
        return std::nullopt;
    }
}

std::string FormatCalc(double v) {
    if (!std::isfinite(v)) return "Error";
    if (v == 0.0) v = 0.0;  // collapse -0 to 0
    char buf[40];
    const double mag = std::fabs(v);
    if (mag != 0.0 && (mag >= 1e12 || mag < 1e-9)) {
        std::snprintf(buf, sizeof(buf), "%.9g", v);  // scientific for extremes
    } else {
        std::snprintf(buf, sizeof(buf), "%.10g", v);
    }
    return buf;
}

// ---- ImmediateCalc ---------------------------------------------------------

void ImmediateCalc::Digit(int d) {
    if (error_) ClearAll();
    lastWasOp_ = false;
    equalsEcho_.clear();
    if (!typing_) { buf_.clear(); typing_ = true; }
    if (buf_ == "0") buf_.clear();
    if (buf_ == "-0") buf_ = "-";
    buf_ += static_cast<char>('0' + d);
    display_ = std::stod(buf_);
}

void ImmediateCalc::Dot() {
    if (error_) ClearAll();
    lastWasOp_ = false;
    equalsEcho_.clear();
    if (!typing_) { buf_ = "0"; typing_ = true; }
    if (buf_.find('.') == std::string::npos) buf_ += '.';
}

void ImmediateCalc::Reduce(int newPrec, bool rightAssoc) {
    while (!pending_.empty()) {
        const int tp = OpPrec(pending_.back().op);
        if (tp > newPrec || (tp == newPrec && !rightAssoc)) {
            const Pending p = pending_.back();
            pending_.pop_back();
            display_ = Compute(p.val, p.op, display_);
            if (!std::isfinite(display_)) { error_ = true; pending_.clear(); return; }
        } else {
            break;
        }
    }
}

void ImmediateCalc::Op(char op) {
    if (error_) return;
    equalsEcho_.clear();
    // Pressing a second operator with no operand between just swaps the operator.
    if (lastWasOp_ && !pending_.empty()) { pending_.back().op = op; return; }
    Reduce(OpPrec(op), op == '^');  // ^ is right-associative
    if (error_) return;
    pending_.push_back({display_, op});
    typing_ = false;
    lastWasOp_ = true;
    buf_.clear();
}

void ImmediateCalc::Equals() {
    if (error_) return;
    // Build the full "a op b op c = " echo from the pending chain + current entry,
    // so the top line reads back the whole calculation after '='.
    std::string e;
    for (const auto& p : pending_) { e += FormatCalc(p.val); e += ' '; e += OpGlyph(p.op); e += ' '; }
    e += FormatCalc(display_);
    e += " =";
    Reduce(0, false);  // collapse the whole stack
    equalsEcho_ = error_ ? std::string() : e;
    lastWasOp_ = false;
    typing_ = false;
    buf_.clear();
}

void ImmediateCalc::Percent() {
    if (error_) return;
    lastWasOp_ = false;
    equalsEcho_.clear();
    display_ /= 100.0;
    buf_ = FormatCalc(display_);
    typing_ = false;
}

void ImmediateCalc::Negate() {
    if (error_) return;
    lastWasOp_ = false;
    equalsEcho_.clear();
    display_ = -display_;
    if (typing_) buf_ = FormatCalc(display_);
}

void ImmediateCalc::Func(const std::string& fn, AngleMode angle) {
    if (error_) ClearAll();
    lastWasOp_ = false;
    equalsEcho_.clear();
    bool ok = false;
    const double r = ApplyUnaryFunc(fn, display_, angle, ok);
    if (!ok) return;  // unknown function: ignore the key
    display_ = r;
    if (!std::isfinite(display_)) error_ = true;
    typing_ = false;
    buf_.clear();
}

void ImmediateCalc::SetConst(double v) {
    if (error_) ClearAll();
    lastWasOp_ = false;
    equalsEcho_.clear();
    display_ = v;
    typing_ = false;
    buf_.clear();
}

void ImmediateCalc::Backspace() {
    if (error_) { ClearAll(); return; }
    if (!typing_ || buf_.empty()) return;
    buf_.pop_back();
    if (buf_.empty() || buf_ == "-") { buf_ = "0"; display_ = 0; typing_ = false; return; }
    try { display_ = std::stod(buf_); } catch (...) { display_ = 0; }
}

void ImmediateCalc::ClearEntry() {
    display_ = 0;
    buf_.clear();
    typing_ = false;
    lastWasOp_ = false;
    equalsEcho_.clear();
    error_ = false;
}

void ImmediateCalc::ClearAll() {
    display_ = 0;
    typing_ = false;
    lastWasOp_ = false;
    error_ = false;
    buf_.clear();
    equalsEcho_.clear();
    pending_.clear();
}

std::string ImmediateCalc::Display() const {
    if (error_) return "Error";
    if (typing_ && !buf_.empty()) return buf_;
    return FormatCalc(display_);
}

std::string ImmediateCalc::Echo() const {
    if (error_) return "";
    if (!equalsEcho_.empty()) return equalsEcho_;
    // The pending AOS chain, e.g. "3 + 4 × " -- the "previous" context above the
    // current entry, matching how a two-line scientific calculator reads back.
    std::string s;
    for (const auto& p : pending_) {
        s += FormatCalc(p.val);
        s += ' ';
        s += OpGlyph(p.op);
        s += ' ';
    }
    return s;
}

}  // namespace superwin
