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

TEST_CASE("EvaluateCalc implicit multiplication", "[calc]") {
    REQUIRE_THAT(E("2pi"), WithinAbs(6.28318530718, 1e-9));     // 2*pi
    REQUIRE_THAT(E("2(3+1)"), WithinAbs(8.0, 1e-12));           // 2*(3+1)
    REQUIRE_THAT(E("(1+2)(3+4)"), WithinAbs(21.0, 1e-12));      // adjacent parens
    REQUIRE_THAT(E("3sin(0)+1"), WithinAbs(1.0, 1e-12));        // 3*sin(0)+1
    REQUIRE_THAT(E("2sqrt(9)"), WithinAbs(6.0, 1e-12));         // 2*sqrt(9)
    REQUIRE_THAT(E("2pi^2"), WithinAbs(19.7392088022, 1e-7));   // 2*(pi^2), not (2pi)^2
}

TEST_CASE("EvaluateCalc functions and constants", "[calc]") {
    REQUIRE_THAT(E("sqrt(16)"), WithinAbs(4.0, 1e-12));
    REQUIRE_THAT(E("ln(e)"), WithinAbs(1.0, 1e-12));
    REQUIRE_THAT(E("log(1000)"), WithinAbs(3.0, 1e-12));
    REQUIRE_THAT(E("sin(0)"), WithinAbs(0.0, 1e-12));
    REQUIRE_THAT(E("cos(pi)"), WithinAbs(-1.0, 1e-12));
}

TEST_CASE("EvaluateCalc supports the Class II 2nd functions", "[calc]") {
    REQUIRE_THAT(E("asinh(0)"), WithinAbs(0.0, 1e-12));
    REQUIRE_THAT(E("asinh(sinh(2))"), WithinAbs(2.0, 1e-9));
    REQUIRE_THAT(E("acosh(cosh(2))"), WithinAbs(2.0, 1e-9));
    REQUIRE_THAT(E("atanh(tanh(0.5))"), WithinAbs(0.5, 1e-9));
    REQUIRE_THAT(E("exp10(3)"), WithinAbs(1000.0, 1e-9));   // 10ˣ
    REQUIRE_THAT(E("exp2(10)"), WithinAbs(1024.0, 1e-9));   // 2ˣ
    REQUIRE_THAT(E("log2(8)"), WithinAbs(3.0, 1e-12));      // digit-suffixed names parse
    REQUIRE_THAT(E("log10(100)"), WithinAbs(2.0, 1e-12));
    REQUIRE_THAT(E("2pi"), WithinAbs(6.28318530718, 1e-9)); // implicit mult unaffected
    REQUIRE_THAT(E("cube(4)"), WithinAbs(64.0, 1e-12));     // x³
    REQUIRE_THAT(E("5^-1"), WithinAbs(0.2, 1e-12));         // x⁻¹ appends ^-1
    REQUIRE_THAT(E("10^(2)"), WithinAbs(100.0, 1e-12));     // 10ˣ cursor insert
}

TEST_CASE("EvaluateCalc honors angle mode", "[calc]") {
    REQUIRE_THAT(E("sin(90)", AngleMode::Degrees), WithinAbs(1.0, 1e-12));
    REQUIRE_THAT(E("cos(180)", AngleMode::Degrees), WithinAbs(-1.0, 1e-12));
    REQUIRE_THAT(E("asin(1)", AngleMode::Degrees), WithinAbs(90.0, 1e-9));
}

TEST_CASE("EvaluateCalc resolves Ans to the previous result", "[calc]") {
    std::string err;
    auto r = EvaluateCalc("Ans+2", AngleMode::Radians, err, 5.0);
    REQUIRE(r.has_value());
    REQUIRE_THAT(*r, WithinAbs(7.0, 1e-12));
    r = EvaluateCalc("2Ans", AngleMode::Radians, err, 5.0);  // implicit multiplication
    REQUIRE(r.has_value());
    REQUIRE_THAT(*r, WithinAbs(10.0, 1e-12));
    r = EvaluateCalc("ans^2", AngleMode::Radians, err, 3.0);  // case-insensitive spellings
    REQUIRE(r.has_value());
    REQUIRE_THAT(*r, WithinAbs(9.0, 1e-12));
    r = EvaluateCalc("Ans", AngleMode::Radians, err);  // defaults to 0
    REQUIRE(r.has_value());
    REQUIRE_THAT(*r, WithinAbs(0.0, 1e-12));
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

TEST_CASE("ImmediateCalc honors operator precedence (AOS)", "[calc][immediate]") {
    // TI-30Xa AOS: multiplication binds tighter than addition even in immediate mode.
    ImmediateCalc a;
    a.Digit(3); a.Op('+'); a.Digit(4); a.Op('*'); a.Digit(2); a.Equals();
    REQUIRE(a.Display() == "11");  // 3 + (4*2)

    ImmediateCalc b;
    b.Digit(2); b.Op('+'); b.Digit(3); b.Op('*'); b.Digit(4); b.Op('-'); b.Digit(1); b.Equals();
    REQUIRE(b.Display() == "13");  // 2 + 12 - 1

    ImmediateCalc c;  // power is right-associative
    c.Digit(2); c.Op('^'); c.Digit(3); c.Op('^'); c.Digit(2); c.Equals();
    REQUIRE(c.Display() == "512");  // 2^(3^2)

    ImmediateCalc d;  // pressing two operators in a row swaps, not double-applies
    d.Digit(8); d.Op('+'); d.Op('*'); d.Digit(2); d.Equals();
    REQUIRE(d.Display() == "16");  // 8 * 2
}

TEST_CASE("ImmediateCalc Echo shows the pending operation and the '=' read-back", "[calc][immediate]") {
    ImmediateCalc c;
    c.Digit(8); c.Op('*');
    REQUIRE(c.Echo() == "8 \xC3\x97 ");   // "8 × " above the entry
    c.Digit(9);
    REQUIRE(c.Display() == "9");
    REQUIRE(c.Echo() == "8 \xC3\x97 ");   // pending op persists while typing
    c.Equals();
    REQUIRE(c.Display() == "72");
    REQUIRE(c.Echo() == "8 \xC3\x97 9 =");  // full read-back after '='
    c.Digit(5);
    REQUIRE(c.Echo().empty());             // a fresh entry clears the echo

    ImmediateCalc d;  // multi-term AOS chain
    d.Digit(3); d.Op('+'); d.Digit(4); d.Op('*');
    REQUIRE(d.Echo() == "3 + 4 \xC3\x97 ");
}

TEST_CASE("ImmediateCalc functions act on the shown value", "[calc][immediate]") {
    ImmediateCalc c;
    c.Digit(9); c.Func("sqrt", AngleMode::Radians);
    REQUIRE(c.Display() == "3");
    c.Func("sqr", AngleMode::Radians);
    REQUIRE(c.Display() == "9");
}

TEST_CASE("ImmediateCalc ClearEntry drops the entry but keeps the pending chain", "[calc][immediate]") {
    ImmediateCalc c;
    c.Digit(8); c.Op('+'); c.Digit(5);
    c.ClearEntry();                    // CE: wipe the mistyped 5...
    REQUIRE(c.Display() == "0");
    REQUIRE(c.Echo() == "8 + ");       // ...but 8 + is still pending
    c.Digit(3); c.Equals();
    REQUIRE(c.Display() == "11");      // 8 + 3
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
