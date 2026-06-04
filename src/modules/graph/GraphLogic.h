// Thin numeric/CAS facade over the Expr engine for the Graphing Calculator.
// Pure logic in superwin_core (unit-tested).
#pragma once

#include <functional>
#include <optional>
#include <string>

namespace superwin {

using GraphFn = std::function<double(double)>;

// Compile `expr` into a numeric callable. nullopt + `error` on failure.
std::optional<GraphFn> CompileExpression(const std::string& expr, std::string& error);

// Convenience for tests: compile and evaluate at a single x.
std::optional<double> EvaluateExpression(const std::string& expr, double x);

// CAS: symbolic derivative (d/dx), simplified and pretty-printed. nullopt on
// parse error (message in `error`).
std::optional<std::string> DifferentiateExpr(const std::string& expr, std::string& error);

// CAS: constant-fold + identity simplification, pretty-printed.
std::optional<std::string> SimplifyExpr(const std::string& expr, std::string& error);

// CAS: symbolic antiderivative (∫ dx), as a re-parsable string. nullopt if there
// is no closed form we handle (or on parse error).
std::optional<std::string> IntegrateExpr(const std::string& expr, std::string& error);

// Unicode "pretty math" rendering of the expression (x², √, ·, …). nullopt on
// parse error.
std::optional<std::string> PrettyExpr(const std::string& expr, std::string& error);

// Numeric definite integral over [a, b] via composite Simpson's rule. nullopt on
// parse error.
std::optional<double> DefiniteIntegral(const std::string& expr, double a, double b);

}  // namespace superwin
