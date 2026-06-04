#include "modules/graph/Expr.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace superwin {

enum class Kind { Const, Var, Add, Sub, Mul, Div, Pow, Neg, Func };

struct ExprNode {
    Kind kind;
    double value = 0;             // Const
    std::string func;            // Func name
    std::shared_ptr<const ExprNode> a, b;  // operands
};

namespace {

using NodeP = std::shared_ptr<const ExprNode>;

NodeP Make(Kind k, double v, std::string fn, NodeP a, NodeP b) {
    auto n = std::make_shared<ExprNode>();
    n->kind = k; n->value = v; n->func = std::move(fn); n->a = std::move(a); n->b = std::move(b);
    return n;
}

bool IsConst(const NodeP& n, double& out) {
    if (n && n->kind == Kind::Const) { out = n->value; return true; }
    return false;
}
bool IsConstEq(const NodeP& n, double v) { double c; return IsConst(n, c) && c == v; }

double ApplyFunc(const std::string& f, double v) {
    if (f == "sin") return std::sin(v);
    if (f == "cos") return std::cos(v);
    if (f == "tan") return std::tan(v);
    if (f == "asin") return std::asin(v);
    if (f == "acos") return std::acos(v);
    if (f == "atan") return std::atan(v);
    if (f == "sinh") return std::sinh(v);
    if (f == "cosh") return std::cosh(v);
    if (f == "tanh") return std::tanh(v);
    if (f == "sqrt") return std::sqrt(v);
    if (f == "cbrt") return std::cbrt(v);
    if (f == "abs") return std::fabs(v);
    if (f == "ln") return std::log(v);
    if (f == "log" || f == "log10") return std::log10(v);
    if (f == "exp") return std::exp(v);
    if (f == "floor") return std::floor(v);
    if (f == "ceil") return std::ceil(v);
    if (f == "sign") return (v > 0) - (v < 0);
    return std::nan("");
}

// ---- smart constructors (fold constants + drop identities) ----
NodeP C(double v) { return Make(Kind::Const, v, "", nullptr, nullptr); }
NodeP Var() { return Make(Kind::Var, 0, "", nullptr, nullptr); }

NodeP Neg(NodeP a) {
    double v;
    if (IsConst(a, v)) return C(-v);
    if (a->kind == Kind::Neg) return a->a;
    return Make(Kind::Neg, 0, "", std::move(a), nullptr);
}
NodeP Add(NodeP a, NodeP b) {
    double x, y;
    if (IsConst(a, x) && IsConst(b, y)) return C(x + y);
    if (IsConstEq(a, 0)) return b;
    if (IsConstEq(b, 0)) return a;
    return Make(Kind::Add, 0, "", std::move(a), std::move(b));
}
NodeP Sub(NodeP a, NodeP b) {
    double x, y;
    if (IsConst(a, x) && IsConst(b, y)) return C(x - y);
    if (IsConstEq(b, 0)) return a;
    if (IsConstEq(a, 0)) return Neg(b);
    return Make(Kind::Sub, 0, "", std::move(a), std::move(b));
}
NodeP Mul(NodeP a, NodeP b) {
    double x, y;
    if (IsConstEq(a, 0) || IsConstEq(b, 0)) return C(0);
    if (IsConst(a, x) && IsConst(b, y)) return C(x * y);
    if (IsConstEq(a, 1)) return b;
    if (IsConstEq(b, 1)) return a;
    if (IsConstEq(a, -1)) return Neg(b);
    if (IsConstEq(b, -1)) return Neg(a);
    return Make(Kind::Mul, 0, "", std::move(a), std::move(b));
}
NodeP Div(NodeP a, NodeP b) {
    double x, y;
    if (IsConstEq(a, 0)) return C(0);
    if (IsConstEq(b, 1)) return a;
    if (IsConst(a, x) && IsConst(b, y) && y != 0) return C(x / y);
    return Make(Kind::Div, 0, "", std::move(a), std::move(b));
}
NodeP Pow(NodeP a, NodeP b) {
    double x, y;
    if (IsConstEq(b, 0)) return C(1);
    if (IsConstEq(b, 1)) return a;
    if (IsConstEq(a, 1)) return C(1);
    if (IsConst(a, x) && IsConst(b, y)) return C(std::pow(x, y));
    return Make(Kind::Pow, 0, "", std::move(a), std::move(b));
}
NodeP Fn(std::string f, NodeP a) {
    double v;
    if (IsConst(a, v)) return C(ApplyFunc(f, v));
    return Make(Kind::Func, 0, std::move(f), std::move(a), nullptr);
}

// ---- parser ----
class Parser {
public:
    explicit Parser(const std::string& s) : s_(s) {}
    NodeP Parse() {
        NodeP n = Expr();
        Skip();
        if (pos_ != s_.size()) throw std::runtime_error("unexpected character at position " + std::to_string(pos_ + 1));
        return n;
    }
private:
    const std::string& s_;
    size_t pos_ = 0;
    void Skip() { while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_; }
    char Peek() { Skip(); return pos_ < s_.size() ? s_[pos_] : '\0'; }
    bool Eat(char c) { if (Peek() == c) { ++pos_; return true; } return false; }

    NodeP Expr() {
        NodeP n = Term();
        for (;;) {
            if (Eat('+')) n = Add(n, Term());
            else if (Eat('-')) n = Sub(n, Term());
            else return n;
        }
    }
    NodeP Term() {
        NodeP n = Unary();
        for (;;) {
            if (Eat('*')) n = Mul(n, Unary());
            else if (Eat('/')) n = Div(n, Unary());
            else return n;
        }
    }
    NodeP Unary() {
        if (Eat('+')) return Unary();
        if (Eat('-')) return Neg(Unary());
        return Power();
    }
    NodeP Power() {
        NodeP base = Primary();
        if (Eat('^')) return Pow(base, Unary());  // right-assoc; unary looser than ^
        return base;
    }
    NodeP Primary() {
        char c = Peek();
        if (c == '(') { ++pos_; NodeP n = Expr(); if (!Eat(')')) throw std::runtime_error("missing ')'"); return n; }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') return Number();
        if (std::isalpha(static_cast<unsigned char>(c))) return Ident();
        throw std::runtime_error("unexpected character");
    }
    NodeP Number() {
        Skip();
        size_t start = pos_;
        while (pos_ < s_.size() &&
               (std::isdigit(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '.' ||
                s_[pos_] == 'e' || s_[pos_] == 'E' ||
                ((s_[pos_] == '+' || s_[pos_] == '-') && pos_ > start && (s_[pos_-1] == 'e' || s_[pos_-1] == 'E')))) {
            ++pos_;
        }
        try { return C(std::stod(s_.substr(start, pos_ - start))); }
        catch (...) { throw std::runtime_error("invalid number"); }
    }
    NodeP Ident() {
        Skip();
        size_t start = pos_;
        while (pos_ < s_.size() && std::isalpha(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        std::string id = s_.substr(start, pos_ - start);
        if (id == "x") return Var();
        if (id == "pi") return C(3.14159265358979323846);
        if (id == "e") return C(2.71828182845904523536);
        if (!Eat('(')) throw std::runtime_error("unknown name '" + id + "'");
        NodeP arg = Expr();
        if (!Eat(')')) throw std::runtime_error("missing ')' after " + id);
        if (ApplyFuncKnown(id)) return Fn(id, arg);
        throw std::runtime_error("unknown function '" + id + "'");
    }
    static bool ApplyFuncKnown(const std::string& f) {
        static const char* kFns[] = {"sin","cos","tan","asin","acos","atan","sinh","cosh","tanh",
                                     "sqrt","cbrt","abs","ln","log","log10","exp","floor","ceil","sign"};
        for (auto* n : kFns) if (f == n) return true;
        return false;
    }
};

double Eval(const NodeP& n, double x) {
    switch (n->kind) {
        case Kind::Const: return n->value;
        case Kind::Var:   return x;
        case Kind::Neg:   return -Eval(n->a, x);
        case Kind::Add:   return Eval(n->a, x) + Eval(n->b, x);
        case Kind::Sub:   return Eval(n->a, x) - Eval(n->b, x);
        case Kind::Mul:   return Eval(n->a, x) * Eval(n->b, x);
        case Kind::Div:   return Eval(n->a, x) / Eval(n->b, x);
        case Kind::Pow:   return std::pow(Eval(n->a, x), Eval(n->b, x));
        case Kind::Func:  return ApplyFunc(n->func, Eval(n->a, x));
    }
    return std::nan("");
}

// derivative of the outer function f at argument a (without chain factor)
NodeP DFunc(const std::string& f, const NodeP& a) {
    if (f == "sin") return Fn("cos", a);
    if (f == "cos") return Neg(Fn("sin", a));
    if (f == "tan") return Div(C(1), Pow(Fn("cos", a), C(2)));
    if (f == "asin") return Div(C(1), Fn("sqrt", Sub(C(1), Pow(a, C(2)))));
    if (f == "acos") return Neg(Div(C(1), Fn("sqrt", Sub(C(1), Pow(a, C(2))))));
    if (f == "atan") return Div(C(1), Add(C(1), Pow(a, C(2))));
    if (f == "sinh") return Fn("cosh", a);
    if (f == "cosh") return Fn("sinh", a);
    if (f == "tanh") return Div(C(1), Pow(Fn("cosh", a), C(2)));
    if (f == "sqrt") return Div(C(1), Mul(C(2), Fn("sqrt", a)));
    if (f == "cbrt") return Div(C(1), Mul(C(3), Pow(Fn("cbrt", a), C(2))));
    if (f == "abs") return Div(a, Fn("abs", a));
    if (f == "ln") return Div(C(1), a);
    if (f == "log" || f == "log10") return Div(C(1), Mul(a, C(std::log(10.0))));
    if (f == "exp") return Fn("exp", a);
    return C(0);  // floor/ceil/sign -> 0 a.e.
}

NodeP Deriv(const NodeP& n) {
    switch (n->kind) {
        case Kind::Const: return C(0);
        case Kind::Var:   return C(1);
        case Kind::Neg:   return Neg(Deriv(n->a));
        case Kind::Add:   return Add(Deriv(n->a), Deriv(n->b));
        case Kind::Sub:   return Sub(Deriv(n->a), Deriv(n->b));
        case Kind::Mul:   return Add(Mul(Deriv(n->a), n->b), Mul(n->a, Deriv(n->b)));
        case Kind::Div:   return Div(Sub(Mul(Deriv(n->a), n->b), Mul(n->a, Deriv(n->b))), Pow(n->b, C(2)));
        case Kind::Func:  return Mul(DFunc(n->func, n->a), Deriv(n->a));
        case Kind::Pow: {
            const NodeP& u = n->a; const NodeP& v = n->b;
            double cv, cu;
            if (IsConst(v, cv)) return Mul(Mul(C(cv), Pow(u, C(cv - 1))), Deriv(u));
            if (IsConst(u, cu)) return Mul(Mul(n, C(std::log(cu))), Deriv(v));
            // general: u^v * (v'*ln(u) + v*u'/u)
            return Mul(n, Add(Mul(Deriv(v), Fn("ln", u)), Div(Mul(v, Deriv(u)), u)));
        }
    }
    return C(0);
}

NodeP Simplify(const NodeP& n) {
    switch (n->kind) {
        case Kind::Const:
        case Kind::Var:   return n;
        case Kind::Neg:   return Neg(Simplify(n->a));
        case Kind::Add:   return Add(Simplify(n->a), Simplify(n->b));
        case Kind::Sub:   return Sub(Simplify(n->a), Simplify(n->b));
        case Kind::Mul:   return Mul(Simplify(n->a), Simplify(n->b));
        case Kind::Div:   return Div(Simplify(n->a), Simplify(n->b));
        case Kind::Pow:   return Pow(Simplify(n->a), Simplify(n->b));
        case Kind::Func:  return Fn(n->func, Simplify(n->a));
    }
    return n;
}

// Symbolic integration wrt x over a useful elementary subset. Returns nullptr
// when there is no closed form we handle (the UI falls back to numeric).
NodeP Integrate(const NodeP& n) {
    double c, nn;
    switch (n->kind) {
        case Kind::Const: return Mul(n, Var());                 // c -> c*x
        case Kind::Var:   return Div(Pow(Var(), C(2)), C(2));   // x -> x^2/2
        case Kind::Neg: { auto u = Integrate(n->a); return u ? Neg(u) : nullptr; }
        case Kind::Add: { auto a = Integrate(n->a), b = Integrate(n->b); return (a && b) ? Add(a, b) : nullptr; }
        case Kind::Sub: { auto a = Integrate(n->a), b = Integrate(n->b); return (a && b) ? Sub(a, b) : nullptr; }
        case Kind::Mul: {
            if (IsConst(n->a, c)) { auto u = Integrate(n->b); return u ? Mul(n->a, u) : nullptr; }
            if (IsConst(n->b, c)) { auto u = Integrate(n->a); return u ? Mul(n->b, u) : nullptr; }
            return nullptr;  // no integration by parts
        }
        case Kind::Div: {
            if (IsConst(n->b, c) && c != 0) { auto u = Integrate(n->a); return u ? Div(u, n->b) : nullptr; }
            if (n->b->kind == Kind::Var && IsConst(n->a, c))      // c/x -> c*ln|x|
                return Mul(n->a, Fn("ln", Fn("abs", Var())));
            // c / x^n  ==  c * x^(-n)
            if (IsConst(n->a, c) && n->b->kind == Kind::Pow &&
                n->b->a->kind == Kind::Var && IsConst(n->b->b, nn)) {
                const double p = -nn;
                if (p == -1) return Mul(n->a, Fn("ln", Fn("abs", Var())));
                return Mul(n->a, Div(Pow(Var(), C(p + 1)), C(p + 1)));
            }
            return nullptr;
        }
        case Kind::Pow: {
            if (n->a->kind == Kind::Var && IsConst(n->b, nn)) {   // x^n
                if (nn == -1) return Fn("ln", Fn("abs", Var()));
                return Div(Pow(Var(), C(nn + 1)), C(nn + 1));
            }
            return nullptr;
        }
        case Kind::Func: {
            if (n->a->kind != Kind::Var) return nullptr;          // only f(x)
            const std::string& f = n->func;
            if (f == "sin") return Neg(Fn("cos", Var()));
            if (f == "cos") return Fn("sin", Var());
            if (f == "exp") return Fn("exp", Var());
            if (f == "sinh") return Fn("cosh", Var());
            if (f == "cosh") return Fn("sinh", Var());
            return nullptr;
        }
    }
    return nullptr;
}

int Prec(const NodeP& n) {
    switch (n->kind) {
        case Kind::Const: return n->value < 0 ? 3 : 4;
        case Kind::Var:
        case Kind::Func:  return 4;
        case Kind::Pow:
        case Kind::Neg:   return 3;
        case Kind::Mul:
        case Kind::Div:   return 2;
        case Kind::Add:
        case Kind::Sub:   return 1;
    }
    return 4;
}

std::string Fmt(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%g", v);
    return buf;
}

std::string Str(const NodeP& n, int threshold);

std::string Raw(const NodeP& n) {
    switch (n->kind) {
        case Kind::Const: return Fmt(n->value);
        case Kind::Var:   return "x";
        case Kind::Neg:   return "-" + Str(n->a, 3);
        case Kind::Add:   return Str(n->a, 1) + " + " + Str(n->b, 2);
        case Kind::Sub:   return Str(n->a, 1) + " - " + Str(n->b, 2);
        case Kind::Mul:   return Str(n->a, 2) + "*" + Str(n->b, 3);
        case Kind::Div:   return Str(n->a, 2) + "/" + Str(n->b, 3);
        case Kind::Pow:   return Str(n->a, 4) + "^" + Str(n->b, 3);
        case Kind::Func:  return n->func + "(" + Str(n->a, 0) + ")";
    }
    return "?";
}

std::string Str(const NodeP& n, int threshold) {
    std::string s = Raw(n);
    return Prec(n) < threshold ? "(" + s + ")" : s;
}

// ---- pretty (Unicode math) ----
std::string Super(long long n) {
    static const char* d[10] = {"⁰", "¹", "²", "³", "⁴",
                                "⁵", "⁶", "⁷", "⁸", "⁹"};
    const bool neg = n < 0;
    unsigned long long m = neg ? static_cast<unsigned long long>(-n) : static_cast<unsigned long long>(n);
    std::string digits;
    if (m == 0) digits = "0";
    while (m) { digits.insert(digits.begin(), static_cast<char>('0' + m % 10)); m /= 10; }
    std::string s;
    if (neg) s += "⁻";
    for (char ch : digits) s += d[ch - '0'];
    return s;
}

std::string Pretty(const NodeP& n, int threshold);

std::string PrettyRaw(const NodeP& n) {
    double e;
    switch (n->kind) {
        case Kind::Const: return Fmt(n->value);
        case Kind::Var:   return "x";
        case Kind::Neg:   return "−" + Pretty(n->a, 3);
        case Kind::Add:   return Pretty(n->a, 1) + " + " + Pretty(n->b, 2);
        case Kind::Sub:   return Pretty(n->a, 1) + " − " + Pretty(n->b, 2);
        case Kind::Mul:   return Pretty(n->a, 2) + "·" + Pretty(n->b, 3);
        case Kind::Div:   return Pretty(n->a, 2) + "/" + Pretty(n->b, 3);
        case Kind::Pow:
            if (IsConst(n->b, e) && e == std::floor(e) && std::fabs(e) < 1e9)
                return Pretty(n->a, 4) + Super(static_cast<long long>(e));
            return Pretty(n->a, 4) + "^" + Pretty(n->b, 3);
        case Kind::Func:
            if (n->func == "sqrt") return "√" + Pretty(n->a, 3);
            return n->func + "(" + Pretty(n->a, 0) + ")";
    }
    return "?";
}

std::string Pretty(const NodeP& n, int threshold) {
    std::string s = PrettyRaw(n);
    return Prec(n) < threshold ? "(" + s + ")" : s;
}

}  // namespace

double Expr::eval(double x) const { return node_ ? Eval(node_, x) : std::nan(""); }
Expr Expr::derivative() const { return node_ ? Expr(Deriv(node_)) : Expr(); }
Expr Expr::integral() const {
    if (!node_) return Expr();
    auto i = Integrate(node_);
    return i ? Expr(Simplify(i)) : Expr();
}
Expr Expr::simplify() const { return node_ ? Expr(Simplify(node_)) : Expr(); }
std::string Expr::str() const { return node_ ? Str(node_, 0) : std::string(); }
std::string Expr::pretty() const { return node_ ? Pretty(node_, 0) : std::string(); }

std::optional<Expr> ParseExpr(const std::string& s, std::string& error) {
    if (s.empty()) { error = "empty expression"; return std::nullopt; }
    try {
        Parser p(s);
        return Expr(p.Parse());
    } catch (const std::exception& e) {
        error = e.what();
        return std::nullopt;
    }
}

}  // namespace superwin
