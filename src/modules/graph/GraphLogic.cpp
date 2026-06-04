#include "modules/graph/GraphLogic.h"

#include <cmath>

#include "modules/graph/Expr.h"

namespace superwin {

std::optional<GraphFn> CompileExpression(const std::string& expr, std::string& error) {
    auto e = ParseExpr(expr, error);
    if (!e) return std::nullopt;
    Expr ex = *e;
    return GraphFn([ex](double x) { return ex.eval(x); });
}

std::optional<double> EvaluateExpression(const std::string& expr, double x) {
    std::string err;
    auto fn = CompileExpression(expr, err);
    if (!fn) return std::nullopt;
    return (*fn)(x);
}

std::optional<std::string> DifferentiateExpr(const std::string& expr, std::string& error) {
    auto e = ParseExpr(expr, error);
    if (!e) return std::nullopt;
    return e->derivative().simplify().str();
}

std::optional<std::string> SimplifyExpr(const std::string& expr, std::string& error) {
    auto e = ParseExpr(expr, error);
    if (!e) return std::nullopt;
    return e->simplify().str();
}

std::optional<std::string> IntegrateExpr(const std::string& expr, std::string& error) {
    auto e = ParseExpr(expr, error);
    if (!e) return std::nullopt;
    Expr anti = e->integral();
    if (!anti.valid()) { error = "no elementary antiderivative found"; return std::nullopt; }
    return anti.str();
}

std::optional<std::string> PrettyExpr(const std::string& expr, std::string& error) {
    auto e = ParseExpr(expr, error);
    if (!e) return std::nullopt;
    return e->pretty();
}

std::optional<double> DefiniteIntegral(const std::string& expr, double a, double b) {
    std::string err;
    auto e = ParseExpr(expr, err);
    if (!e) return std::nullopt;
    int n = 1000;                      // even number of intervals
    if (n % 2) ++n;
    const double h = (b - a) / n;
    double sum = e->eval(a) + e->eval(b);
    for (int i = 1; i < n; ++i) {
        const double x = a + i * h;
        sum += (i % 2 ? 4.0 : 2.0) * e->eval(x);
    }
    return sum * h / 3.0;
}

}  // namespace superwin
