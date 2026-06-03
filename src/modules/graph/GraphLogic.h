// Math-expression evaluator for the Graphing Calculator. Pure logic in
// superwin_core (unit-tested). Compiles an f(x) expression once into a callable,
// supporting + - * / ^, parentheses, the variable x, common functions
// (sin, cos, tan, asin, acos, atan, sqrt, abs, ln, log, log10, exp, floor, ceil)
// and the constants pi and e.
#pragma once

#include <functional>
#include <optional>
#include <string>

namespace superwin {

using GraphFn = std::function<double(double)>;

// Compile `expr`. On success returns the callable; on failure returns nullopt and
// fills `error` with a short message.
std::optional<GraphFn> CompileExpression(const std::string& expr, std::string& error);

// Convenience for tests: compile and evaluate at a single x. nullopt on error.
std::optional<double> EvaluateExpression(const std::string& expr, double x);

}  // namespace superwin
