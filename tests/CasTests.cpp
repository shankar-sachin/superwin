#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <string>

#include "modules/graph/Cas.h"
#include "modules/graph/Expr.h"

using namespace superwin;
using Catch::Matchers::WithinAbs;

namespace {
Expr P(const std::string& s) {
    std::string err;
    auto e = ParseExpr(s, err);
    REQUIRE(e.has_value());
    return *e;
}
}  // namespace

TEST_CASE("SolveRoots finds polynomial roots", "[cas]") {
    auto r = SolveRoots(P("x^2 - 4"));
    REQUIRE(r.size() == 2);
    REQUIRE_THAT(r[0], WithinAbs(-2.0, 1e-5));
    REQUIRE_THAT(r[1], WithinAbs(2.0, 1e-5));
}

TEST_CASE("SolveRoots finds a linear root", "[cas]") {
    auto r = SolveRoots(P("2x - 6"));
    REQUIRE(r.size() == 1);
    REQUIRE_THAT(r[0], WithinAbs(3.0, 1e-5));
}

TEST_CASE("SolveRoots finds trig roots in a window", "[cas]") {
    auto r = SolveRoots(P("sin(x)"), -4.0, 4.0, 4000);
    REQUIRE(r.size() == 3);  // -pi, 0, pi
    REQUIRE_THAT(r[0], WithinAbs(-3.14159265, 1e-4));
    REQUIRE_THAT(r[1], WithinAbs(0.0, 1e-4));
    REQUIRE_THAT(r[2], WithinAbs(3.14159265, 1e-4));
}

TEST_CASE("SolveRoots rejects poles (1/x has no root)", "[cas]") {
    auto r = SolveRoots(P("1/x"), -10.0, 10.0, 4000);
    REQUIRE(r.empty());
}

TEST_CASE("SolveRoots handles an expression with no real roots", "[cas]") {
    auto r = SolveRoots(P("x^2 + 1"));
    REQUIRE(r.empty());
}
