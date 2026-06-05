#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "modules/graph/Expr.h"
#include "modules/graph/GraphLogic.h"

using namespace superwin;
using Catch::Matchers::WithinAbs;

namespace {
Expr P(const std::string& s) { std::string e; auto r = ParseExpr(s, e); REQUIRE(r.has_value()); return *r; }

// Verify a symbolic derivative against a central finite difference.
void CheckDerivative(const std::string& expr, double x) {
    Expr f = P(expr);
    Expr d = f.derivative();
    const double h = 1e-6;
    const double approx = (f.eval(x + h) - f.eval(x - h)) / (2 * h);
    REQUIRE_THAT(d.eval(x), WithinAbs(approx, 1e-4));
}
}  // namespace

TEST_CASE("Symbolic derivative matches numeric derivative", "[Expr][CAS]") {
    CheckDerivative("x^2", 3.0);
    CheckDerivative("x^3 + 2*x", 1.5);
    CheckDerivative("sin(x)", 0.7);
    CheckDerivative("cos(x)*x", 1.1);
    CheckDerivative("exp(x)", 0.3);
    CheckDerivative("ln(x)", 2.0);
    CheckDerivative("sqrt(x)", 4.0);
    CheckDerivative("1/x", 2.0);
    CheckDerivative("sin(x^2)", 1.3);
    CheckDerivative("tan(x)", 0.5);
}

TEST_CASE("Simplify drops identities", "[Expr][CAS]") {
    std::string e;
    REQUIRE(SimplifyExpr("x + 0", e).value() == "x");
    REQUIRE(SimplifyExpr("1*x", e).value() == "x");
    REQUIRE(SimplifyExpr("x^1", e).value() == "x");
    REQUIRE(SimplifyExpr("0*x + 5", e).value() == "5");
    REQUIRE(SimplifyExpr("2 + 3", e).value() == "5");
}

TEST_CASE("DifferentiateExpr produces a re-parsable, correct result", "[Expr][CAS]") {
    std::string e;
    auto d = DifferentiateExpr("x^3", e);
    REQUIRE(d.has_value());
    // d/dx x^3 == 3x^2; verify numerically by re-parsing the printed result.
    Expr back = P(*d);
    REQUIRE_THAT(back.eval(2.0), WithinAbs(12.0, 1e-9));

    auto ds = DifferentiateExpr("sin(x)", e);
    REQUIRE(ds.has_value());
    REQUIRE_THAT(P(*ds).eval(0.0), WithinAbs(1.0, 1e-9));  // cos(0)=1
}

TEST_CASE("Parse errors are reported", "[Expr][CAS]") {
    std::string e;
    REQUIRE_FALSE(ParseExpr("x +", e).has_value());
    REQUIRE_FALSE(ParseExpr("foo(x)", e).has_value());
    REQUIRE_FALSE(ParseExpr("", e).has_value());
}

TEST_CASE("Symbolic integral differentiates back to the integrand", "[Expr][CAS]") {
    const char* cases[] = {"x^2", "3*x^2 + 2*x", "sin(x)", "cos(x)", "exp(x)", "1/x^2", "5"};
    for (auto* c : cases) {
        Expr f = P(c);
        Expr F = f.integral();
        REQUIRE(F.valid());
        // d/dx of the antiderivative should recover f (check a few points).
        Expr back = F.derivative();
        for (double x : {0.6, 1.7, 2.9}) {
            REQUIRE_THAT(back.eval(x), WithinAbs(f.eval(x), 1e-6));
        }
    }
}

TEST_CASE("Integration reports when there's no closed form", "[Expr][CAS]") {
    std::string e;
    REQUIRE_FALSE(IntegrateExpr("sin(x^2)", e).has_value());  // not elementary here
}

TEST_CASE("Numeric definite integral (Simpson)", "[Expr][CAS]") {
    REQUIRE_THAT(DefiniteIntegral("x", 0, 2).value(), WithinAbs(2.0, 1e-6));
    REQUIRE_THAT(DefiniteIntegral("x^2", 0, 3).value(), WithinAbs(9.0, 1e-6));
    REQUIRE_THAT(DefiniteIntegral("sin(x)", 0, 3.14159265358979).value(), WithinAbs(2.0, 1e-4));
}

TEST_CASE("Pretty math uses Unicode", "[Expr][CAS]") {
    std::string e;
    REQUIRE(PrettyExpr("x^2", e).value() == std::string("x²"));
    REQUIRE(PrettyExpr("sqrt(x)", e).value() == std::string("√x"));
}

TEST_CASE("Parser accepts pretty Unicode operators", "[Expr][CAS][unicode]") {
    // The math field's beautified text is itself re-parseable.
    for (double x : {0.3, 1.4, 2.7}) {
        REQUIRE_THAT(P("2·x").eval(x),   WithinAbs(P("2*x").eval(x), 1e-12));   // ·
        REQUIRE_THAT(P("2×x").eval(x),   WithinAbs(P("2*x").eval(x), 1e-12));   // ×
        REQUIRE_THAT(P("x−1").eval(x),   WithinAbs(P("x-1").eval(x), 1e-12));   // −
        REQUIRE_THAT(P("x÷2").eval(x),   WithinAbs(P("x/2").eval(x), 1e-12));   // ÷
        REQUIRE_THAT(P("√x").eval(x),    WithinAbs(P("sqrt(x)").eval(x), 1e-12));
        REQUIRE_THAT(P("x²").eval(x),    WithinAbs(P("x^2").eval(x), 1e-12));
        REQUIRE_THAT(P("x³+x²").eval(x), WithinAbs(P("x^3+x^2").eval(x), 1e-12));
        REQUIRE_THAT(P("x⁻²").eval(x),   WithinAbs(P("x^(-2)").eval(x), 1e-12));
    }
    REQUIRE_THAT(P("π").eval(0.0), WithinAbs(3.14159265358979, 1e-10));  // π
}

TEST_CASE("Typed derivative graphs the derivative", "[Expr][CAS][calc]") {
    for (double x : {0.4, 1.2, 2.5}) {
        REQUIRE_THAT(P("d/dx(x^2)").eval(x),     WithinAbs(2 * x, 1e-9));
        REQUIRE_THAT(P("deriv(x^2)").eval(x),    WithinAbs(2 * x, 1e-9));
        REQUIRE_THAT(P("derivative(x^2)").eval(x), WithinAbs(2 * x, 1e-9));
        REQUIRE_THAT(P("d/dx(sin(x))").eval(x),  WithinAbs(std::cos(x), 1e-9));
        // nested and mixed into a larger expression
        REQUIRE_THAT(P("d/dx(d/dx(x^3))").eval(x), WithinAbs(6 * x, 1e-9));
        REQUIRE_THAT(P("d/dx(x^2)+1").eval(x),     WithinAbs(2 * x + 1, 1e-9));
    }
}

TEST_CASE("Typed integral graphs an antiderivative", "[Expr][CAS][calc]") {
    // Closed-form: int(x) = x^2/2, int(cos(x)) = sin(x) (constant of integration
    // is fixed so the antiderivative passes through 0 at x=0 for these).
    for (double x : {0.4, 1.2, 2.5}) {
        REQUIRE_THAT(P("int(x)").eval(x),       WithinAbs(x * x / 2, 1e-9));
        REQUIRE_THAT(P("integral(x)dx").eval(x), WithinAbs(x * x / 2, 1e-9));
        REQUIRE_THAT(P("∫(cos(x))dx").eval(x),  WithinAbs(std::sin(x), 1e-9));
        // d/dx of a typed integral recovers the integrand.
        REQUIRE_THAT(P("d/dx(int(sin(x)))").eval(x), WithinAbs(std::sin(x), 1e-9));
    }
}

TEST_CASE("Typed integral falls back to numeric antiderivative", "[Expr][CAS][calc]") {
    // exp(x^2) has no elementary antiderivative -> NumInt node, F(x)=∫₀ˣ f dt.
    Expr g = P("int(exp(x^2))");
    for (double x : {0.5, 1.0, 1.5}) {
        const double ref = DefiniteIntegral("exp(x^2)", 0, x).value();
        REQUIRE_THAT(g.eval(x), WithinAbs(ref, 1e-4));
    }
    REQUIRE_THAT(g.eval(0.0), WithinAbs(0.0, 1e-12));  // F(0)=0
}
