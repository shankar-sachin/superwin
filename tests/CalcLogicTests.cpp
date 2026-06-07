#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>

#include "modules/calc/CalcLogic.h"

using namespace superwin;
using Catch::Matchers::WithinAbs;

namespace {
double E(const std::string& s, AngleMode a = AngleMode::Radians) {
    std::string err;
    auto r = EvaluateCalc(s, a, err);
    REQUIRE(r.has_value());
    return *r;
}
}  // namespace

TEST_CASE("EvaluateCalc arithmetic and precedence", "[calc]") {
    REQUIRE_THAT(E("1+2*3"), WithinAbs(7.0, 1e-12));
    REQUIRE_THAT(E("(1+2)*3"), WithinAbs(9.0, 1e-12));
    REQUIRE_THAT(E("2^3^2"), WithinAbs(512.0, 1e-9));   // right-associative
    REQUIRE_THAT(E("-3^2"), WithinAbs(-9.0, 1e-9));     // unary looser than ^
    REQUIRE_THAT(E("10/4"), WithinAbs(2.5, 1e-12));
    REQUIRE_THAT(E("50%"), WithinAbs(0.5, 1e-12));      // postfix percent
    REQUIRE_THAT(E("5!"), WithinAbs(120.0, 1e-9));      // postfix factorial
}

TEST_CASE("EvaluateCalc functions and constants", "[calc]") {
    REQUIRE_THAT(E("sqrt(16)"), WithinAbs(4.0, 1e-12));
    REQUIRE_THAT(E("ln(e)"), WithinAbs(1.0, 1e-12));
    REQUIRE_THAT(E("log(1000)"), WithinAbs(3.0, 1e-12));
    REQUIRE_THAT(E("sin(0)"), WithinAbs(0.0, 1e-12));
    REQUIRE_THAT(E("cos(pi)"), WithinAbs(-1.0, 1e-12));
}

TEST_CASE("EvaluateCalc honors angle mode", "[calc]") {
    REQUIRE_THAT(E("sin(90)", AngleMode::Degrees), WithinAbs(1.0, 1e-12));
    REQUIRE_THAT(E("cos(180)", AngleMode::Degrees), WithinAbs(-1.0, 1e-12));
    REQUIRE_THAT(E("asin(1)", AngleMode::Degrees), WithinAbs(90.0, 1e-9));
}

TEST_CASE("EvaluateCalc reports errors", "[calc]") {
    std::string err;
    REQUIRE_FALSE(EvaluateCalc("1+", AngleMode::Radians, err).has_value());
    REQUIRE_FALSE(EvaluateCalc("", AngleMode::Radians, err).has_value());
    REQUIRE_FALSE(EvaluateCalc("1/0", AngleMode::Radians, err).has_value());  // non-finite
    REQUIRE_FALSE(EvaluateCalc("frob(2)", AngleMode::Radians, err).has_value());
}

TEST_CASE("FormatCalc trims and handles edge cases", "[calc]") {
    REQUIRE(FormatCalc(0.0) == "0");
    REQUIRE(FormatCalc(-0.0) == "0");
    REQUIRE(FormatCalc(2.5) == "2.5");
    REQUIRE(FormatCalc(std::nan("")) == "Error");
    REQUIRE(FormatCalc(std::numeric_limits<double>::infinity()) == "Error");
}

TEST_CASE("ImmediateCalc immediate-execution chain", "[calc][immediate]") {
    ImmediateCalc c;
    // 3 + 4 = 7
    c.Digit(3); c.Op('+'); c.Digit(4); c.Equals();
    REQUIRE(c.Display() == "7");
    // continuing: + 4 + chains the running value (7 + 4 = 11)
    ImmediateCalc d;
    d.Digit(7); d.Op('+'); d.Digit(4); d.Op('+'); // 11 shown after second +
    REQUIRE(d.Display() == "11");
    d.Digit(2); d.Equals();                       // 11 + 2 = 13
    REQUIRE(d.Display() == "13");
}

TEST_CASE("ImmediateCalc functions act on the shown value", "[calc][immediate]") {
    ImmediateCalc c;
    c.Digit(9); c.Func("sqrt", AngleMode::Radians);
    REQUIRE(c.Display() == "3");
    c.Func("sqr", AngleMode::Radians);
    REQUIRE(c.Display() == "9");
}

TEST_CASE("ImmediateCalc clear and error states", "[calc][immediate]") {
    ImmediateCalc c;
    c.Digit(5); c.Op('/'); c.Digit(0); c.Equals();
    REQUIRE(c.error());
    REQUIRE(c.Display() == "Error");
    c.ClearAll();
    REQUIRE_FALSE(c.error());
    REQUIRE(c.Display() == "0");
    c.Digit(1); c.Digit(2); c.Backspace();
    REQUIRE(c.Display() == "1");
}
