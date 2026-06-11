#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "modules/graph/GraphLogic.h"

using namespace superwin;
using Catch::Matchers::WithinAbs;

TEST_CASE("EvaluateExpression handles arithmetic and precedence", "[GraphLogic]") {
    REQUIRE(EvaluateExpression("1+2*3", 0).value() == 7.0);
    REQUIRE(EvaluateExpression("(1+2)*3", 0).value() == 9.0);
    REQUIRE(EvaluateExpression("2^3^2", 0).value() == 512.0);  // right-associative
    REQUIRE(EvaluateExpression("-2^2", 0).value() == -4.0);    // unary binds looser than ^
}

TEST_CASE("EvaluateExpression uses the variable x", "[GraphLogic]") {
    REQUIRE(EvaluateExpression("x*x + 1", 3).value() == 10.0);
    REQUIRE(EvaluateExpression("2*x - 5", 4).value() == 3.0);
}

TEST_CASE("EvaluateExpression supports functions and constants", "[GraphLogic]") {
    REQUIRE_THAT(EvaluateExpression("sin(0)", 0).value(), WithinAbs(0.0, 1e-9));
    REQUIRE_THAT(EvaluateExpression("cos(pi)", 0).value(), WithinAbs(-1.0, 1e-9));
    REQUIRE_THAT(EvaluateExpression("sqrt(16)", 0).value(), WithinAbs(4.0, 1e-9));
    REQUIRE_THAT(EvaluateExpression("ln(e)", 0).value(), WithinAbs(1.0, 1e-9));
    REQUIRE_THAT(EvaluateExpression("abs(0-7)", 0).value(), WithinAbs(7.0, 1e-9));
}

TEST_CASE("CompileExpression reports errors", "[GraphLogic]") {
    std::string err;
    REQUIRE_FALSE(CompileExpression("2 +", err).has_value());
    REQUIRE_FALSE(err.empty());
    REQUIRE_FALSE(CompileExpression("sin(x", err).has_value());
    REQUIRE_FALSE(CompileExpression("nope(x)", err).has_value());
    REQUIRE_FALSE(CompileExpression("", err).has_value());
}

TEST_CASE("CompileExpression compiles once, evaluates many", "[GraphLogic]") {
    std::string err;
    auto fn = CompileExpression("x^2", err);
    REQUIRE(fn.has_value());
    REQUIRE((*fn)(2) == 4.0);
    REQUIRE((*fn)(5) == 25.0);
}

TEST_CASE("ConstantValue detects x-free expressions", "[GraphLogic]") {
    REQUIRE_THAT(ConstantValue("3^2").value(), WithinAbs(9.0, 1e-12));
    REQUIRE_THAT(ConstantValue("3.4^2").value(), WithinAbs(11.56, 1e-9));
    REQUIRE_THAT(ConstantValue("pi^3").value(), WithinAbs(31.00627668, 1e-6));
    REQUIRE_THAT(ConstantValue("2+sqrt(2)").value(), WithinAbs(3.41421356237, 1e-9));
    REQUIRE_THAT(ConstantValue("sum(n, 1, 10)").value(), WithinAbs(55.0, 1e-12));  // x-free Σ
    REQUIRE_FALSE(ConstantValue("x^2").has_value());
    REQUIRE_FALSE(ConstantValue("3x").has_value());
    REQUIRE_FALSE(ConstantValue("int(sin(x^2))").has_value());  // numeric ∫₀ˣ depends on x
    REQUIRE_FALSE(ConstantValue("1/0").has_value());            // non-finite
    REQUIRE_FALSE(ConstantValue("2+").has_value());             // parse error
}

TEST_CASE("RowResultText shows numeric answers for constant rows", "[GraphLogic]") {
    REQUIRE(RowResultText("3^2", false).value() == "= 9");
    REQUIRE(RowResultText("3^2", true).value() == "= 9");
    REQUIRE(RowResultText("3.4^2", false).value() == "= 11.56");
    REQUIRE_FALSE(RowResultText("x^2", false).has_value());   // a curve, no popup
    REQUIRE_FALSE(RowResultText("2+", true).has_value());     // parse error
}

TEST_CASE("RowResultText shows resolved calculus only in CAS mode", "[GraphLogic]") {
    // deriv((x^3)) is what LatexToInfix emits for d/dx; it resolves at parse time.
    auto cas = RowResultText("deriv((x^3))", true);
    REQUIRE(cas.has_value());
    REQUIRE(cas->find("x") != std::string::npos);  // an expression, e.g. "= 3x²"
    REQUIRE_FALSE(RowResultText("deriv((x^3))", false).has_value());  // Class III: none
    REQUIRE(RowResultText("int(x^2)", true).has_value());
    // Constant calculus still resolves to a number, CAS or not.
    REQUIRE(RowResultText("sum(n, 1, 10)", true).value() == "= 55");
}
