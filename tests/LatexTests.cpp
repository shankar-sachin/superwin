#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <string>

#include "modules/graph/Latex.h"
#include "modules/graph/Expr.h"

using namespace superwin;
using Catch::Matchers::WithinAbs;

namespace {
// Translate LaTeX -> infix, then parse + evaluate at x.
double EvalLatex(const std::string& latex, double x) {
    std::string err;
    auto e = ParseExpr(LatexToInfix(latex), err);
    REQUIRE(e.has_value());
    return e->eval(x);
}
}  // namespace

TEST_CASE("LaTeX maps basic structures to infix", "[Latex]") {
    CHECK(LatexToInfix("x^2") == "x^(2)");
    CHECK(LatexToInfix("\\frac{1}{x}") == "((1)/(x))");
    CHECK(LatexToInfix("\\sqrt{x}") == "sqrt(x)");
    CHECK(LatexToInfix("\\pi") == "pi");
    CHECK(LatexToInfix("\\sin\\left(x\\right)") == "sin(x)");
    CHECK(LatexToInfix("x\\cdot x") == "x*x");
}

TEST_CASE("LaTeX evaluates equivalently to the math it renders", "[Latex]") {
    REQUIRE_THAT(EvalLatex("x^2", 3.0), WithinAbs(9.0, 1e-9));
    REQUIRE_THAT(EvalLatex("2x", 3.0), WithinAbs(6.0, 1e-9));            // implicit mult
    REQUIRE_THAT(EvalLatex("2\\sin\\left(x\\right)", 0.0), WithinAbs(0.0, 1e-9));
    REQUIRE_THAT(EvalLatex("\\frac{x^2}{2}", 4.0), WithinAbs(8.0, 1e-9));
    REQUIRE_THAT(EvalLatex("\\sqrt[3]{x}", 27.0), WithinAbs(3.0, 1e-9)); // cube root
    REQUIRE_THAT(EvalLatex("\\left(x+1\\right)\\left(x-1\\right)", 3.0), WithinAbs(8.0, 1e-9));
}

TEST_CASE("LaTeX summations translate and evaluate", "[Latex]") {
    // Σ_{n=1}^{3} x^n at x=2 -> 2 + 4 + 8 = 14
    REQUIRE_THAT(EvalLatex("\\sum_{n=1}^{3}x^{n}", 2.0), WithinAbs(14.0, 1e-9));
}

TEST_CASE("LaTeX integral graphs an antiderivative", "[Latex]") {
    // ∫ cos(x) dx -> sin(x)
    REQUIRE_THAT(EvalLatex("\\int\\cos\\left(x\\right)dx", 1.0), WithinAbs(std::sin(1.0), 1e-7));
}
