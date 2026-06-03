#include "modules/graph/GraphLogic.h"

#include <cctype>
#include <cmath>
#include <stdexcept>

namespace superwin {
namespace {

// Recursive-descent parser that compiles an expression into a GraphFn closure.
// Grammar (lowest to highest precedence):
//   expr   := term (('+'|'-') term)*
//   term   := power (('*'|'/') power)*
//   power  := unary ('^' power)?          (right associative)
//   unary  := ('+'|'-') unary | primary
//   primary:= number | 'x' | ident '(' expr ')' | ident(const) | '(' expr ')'
class Parser {
public:
    explicit Parser(const std::string& s) : s_(s) {}

    GraphFn Parse() {
        GraphFn fn = ParseExpr();
        SkipSpaces();
        if (pos_ != s_.size()) throw std::runtime_error("unexpected character at position " + std::to_string(pos_ + 1));
        return fn;
    }

private:
    const std::string& s_;
    size_t pos_ = 0;

    void SkipSpaces() { while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_; }
    char Peek() { SkipSpaces(); return pos_ < s_.size() ? s_[pos_] : '\0'; }
    bool Accept(char c) { if (Peek() == c) { ++pos_; return true; } return false; }

    GraphFn ParseExpr() {
        GraphFn left = ParseTerm();
        for (;;) {
            if (Accept('+')) { auto r = ParseTerm(); auto l = left; left = [l, r](double x) { return l(x) + r(x); }; }
            else if (Accept('-')) { auto r = ParseTerm(); auto l = left; left = [l, r](double x) { return l(x) - r(x); }; }
            else return left;
        }
    }

    GraphFn ParseTerm() {
        GraphFn left = ParseUnary();
        for (;;) {
            if (Accept('*')) { auto r = ParseUnary(); auto l = left; left = [l, r](double x) { return l(x) * r(x); }; }
            else if (Accept('/')) { auto r = ParseUnary(); auto l = left; left = [l, r](double x) { return l(x) / r(x); }; }
            else return left;
        }
    }

    // Unary +/- binds looser than '^', so -2^2 == -(2^2) (the usual convention).
    GraphFn ParseUnary() {
        if (Accept('+')) return ParseUnary();
        if (Accept('-')) { auto u = ParseUnary(); return [u](double x) { return -u(x); }; }
        return ParsePower();
    }

    GraphFn ParsePower() {
        GraphFn base = ParsePrimary();
        if (Accept('^')) {
            GraphFn exp = ParseUnary();  // right associative; allows 2^-3
            return [base, exp](double x) { return std::pow(base(x), exp(x)); };
        }
        return base;
    }

    GraphFn ParsePrimary() {
        const char c = Peek();
        if (c == '(') {
            ++pos_;
            GraphFn e = ParseExpr();
            if (!Accept(')')) throw std::runtime_error("missing ')'");
            return e;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') return ParseNumber();
        if (std::isalpha(static_cast<unsigned char>(c))) return ParseIdentifier();
        throw std::runtime_error("unexpected character");
    }

    GraphFn ParseNumber() {
        SkipSpaces();
        size_t start = pos_;
        while (pos_ < s_.size() &&
               (std::isdigit(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '.' ||
                s_[pos_] == 'e' || s_[pos_] == 'E' ||
                ((s_[pos_] == '+' || s_[pos_] == '-') && pos_ > start &&
                 (s_[pos_ - 1] == 'e' || s_[pos_ - 1] == 'E')))) {
            ++pos_;
        }
        try {
            const double v = std::stod(s_.substr(start, pos_ - start));
            return [v](double) { return v; };
        } catch (...) {
            throw std::runtime_error("invalid number");
        }
    }

    GraphFn ParseIdentifier() {
        SkipSpaces();
        size_t start = pos_;
        while (pos_ < s_.size() && std::isalpha(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        std::string id = s_.substr(start, pos_ - start);

        if (id == "x") return [](double x) { return x; };
        if (id == "pi") return [](double) { return 3.14159265358979323846; };
        if (id == "e")  return [](double) { return 2.71828182845904523536; };

        // Otherwise it must be a function call: ident '(' expr ')'.
        if (!Accept('(')) throw std::runtime_error("unknown name '" + id + "'");
        GraphFn arg = ParseExpr();
        if (!Accept(')')) throw std::runtime_error("missing ')' after " + id);

        if (id == "sin")   return [arg](double x) { return std::sin(arg(x)); };
        if (id == "cos")   return [arg](double x) { return std::cos(arg(x)); };
        if (id == "tan")   return [arg](double x) { return std::tan(arg(x)); };
        if (id == "asin")  return [arg](double x) { return std::asin(arg(x)); };
        if (id == "acos")  return [arg](double x) { return std::acos(arg(x)); };
        if (id == "atan")  return [arg](double x) { return std::atan(arg(x)); };
        if (id == "sqrt")  return [arg](double x) { return std::sqrt(arg(x)); };
        if (id == "abs")   return [arg](double x) { return std::fabs(arg(x)); };
        if (id == "ln")    return [arg](double x) { return std::log(arg(x)); };
        if (id == "log" || id == "log10") return [arg](double x) { return std::log10(arg(x)); };
        if (id == "exp")   return [arg](double x) { return std::exp(arg(x)); };
        if (id == "floor") return [arg](double x) { return std::floor(arg(x)); };
        if (id == "ceil")  return [arg](double x) { return std::ceil(arg(x)); };
        throw std::runtime_error("unknown function '" + id + "'");
    }
};

}  // namespace

std::optional<GraphFn> CompileExpression(const std::string& expr, std::string& error) {
    if (expr.empty()) { error = "empty expression"; return std::nullopt; }
    try {
        Parser p(expr);
        return p.Parse();
    } catch (const std::exception& e) {
        error = e.what();
        return std::nullopt;
    }
}

std::optional<double> EvaluateExpression(const std::string& expr, double x) {
    std::string err;
    auto fn = CompileExpression(expr, err);
    if (!fn) return std::nullopt;
    return (*fn)(x);
}

}  // namespace superwin
