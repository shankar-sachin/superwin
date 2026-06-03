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
