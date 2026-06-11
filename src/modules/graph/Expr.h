// A small symbolic-math engine (CAS) for the Graphing Calculator. Pure logic in
// superwin_core (unit-tested). Parses an expression into an AST, then evaluates
// numerically, differentiates symbolically (d/dx), simplifies, and pretty-prints.
//
// Grammar: + - * / ^, unary +/-, parentheses, the variable x, the constants pi
// and e, and functions sin cos tan asin acos atan sinh cosh tanh sqrt cbrt abs
// ln log exp floor ceil sign.
#pragma once

#include <memory>
#include <optional>
#include <string>

namespace superwin {

struct ExprNode;  // opaque; defined in Expr.cpp

class Expr {
public:
    Expr() = default;
    explicit Expr(std::shared_ptr<const ExprNode> n) : node_(std::move(n)) {}

    bool        valid() const { return node_ != nullptr; }
    bool        isConstant() const;          // folds to a single number (no x)
    double      eval(double x) const;        // numeric value at x
    Expr        derivative() const;          // d/dx, lightly simplified
    Expr        integral() const;            // ∫ dx; invalid Expr if no closed form
    Expr        simplify() const;            // constant-fold + identities
    std::string str() const;                 // plain infix (re-parsable)
    std::string pretty() const;              // Unicode math (x², √, ·, …)

    std::shared_ptr<const ExprNode> node_;
};

// Parse `s`. On failure returns nullopt and fills `error`.
std::optional<Expr> ParseExpr(const std::string& s, std::string& error);

}  // namespace superwin
