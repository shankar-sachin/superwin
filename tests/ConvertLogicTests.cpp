#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "modules/convert/ConvertLogic.h"

using namespace superwin;

namespace {
bool Near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }
}

TEST_CASE("Linear unit conversions", "[convert]") {
    auto km = ConvertUnit(UnitCategory::Length, "Kilometers", "Meters", 1.0);
    REQUIRE(km.has_value());
    CHECK(Near(*km, 1000.0));

    auto inch = ConvertUnit(UnitCategory::Length, "Inches", "Centimeters", 1.0);
    REQUIRE(inch.has_value());
    CHECK(Near(*inch, 2.54));

    auto gb = ConvertUnit(UnitCategory::DataSize, "Gigabytes", "Megabytes", 1.0);
    REQUIRE(gb.has_value());
    CHECK(Near(*gb, 1024.0));

    auto lb = ConvertUnit(UnitCategory::Mass, "Pounds", "Grams", 1.0);
    REQUIRE(lb.has_value());
    CHECK(Near(*lb, 453.59237, 1e-3));
}

TEST_CASE("Temperature conversions", "[convert]") {
    auto f = ConvertUnit(UnitCategory::Temperature, "Celsius", "Fahrenheit", 100.0);
    REQUIRE(f.has_value());
    CHECK(Near(*f, 212.0));

    auto k = ConvertUnit(UnitCategory::Temperature, "Celsius", "Kelvin", 0.0);
    REQUIRE(k.has_value());
    CHECK(Near(*k, 273.15));

    auto c = ConvertUnit(UnitCategory::Temperature, "Fahrenheit", "Celsius", 32.0);
    REQUIRE(c.has_value());
    CHECK(Near(*c, 0.0));
}

TEST_CASE("Unknown units return nullopt", "[convert]") {
    CHECK_FALSE(ConvertUnit(UnitCategory::Length, "Furlongs", "Meters", 1.0).has_value());
}

TEST_CASE("Number base parse/format round-trips", "[convert]") {
    CHECK(ParseInBase("255", 10) == 255);
    CHECK(ParseInBase("ff", 16) == 255);
    CHECK(ParseInBase("11111111", 2) == 255);
    CHECK(ParseInBase("-10", 16) == -16);
    CHECK_FALSE(ParseInBase("xyz", 10).has_value());
    CHECK_FALSE(ParseInBase("2", 2).has_value());  // '2' invalid in binary

    CHECK(FormatInBase(255, 16) == "FF");
    CHECK(FormatInBase(255, 2) == "11111111");
    CHECK(FormatInBase(0, 16) == "0");
    CHECK(FormatInBase(-16, 16) == "-10");
}
