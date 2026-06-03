#include <catch2/catch_test_macros.hpp>

#include "modules/jsonfmt/JsonLogic.h"

using namespace superwin;

TEST_CASE("FormatJson pretty-prints valid JSON", "[JsonLogic]") {
    const auto r = FormatJson(R"({"b":1,"a":[1,2]})", 2);
    REQUIRE(r.ok);
    REQUIRE(r.error.empty());
    // Pretty output spans multiple lines and uses 2-space indentation.
    REQUIRE(r.text.find('\n') != std::string::npos);
    REQUIRE(r.text.find("  \"b\"") != std::string::npos);
}

TEST_CASE("FormatJson minifies with negative indent", "[JsonLogic]") {
    const auto r = FormatJson("{ \"a\" : 1 ,\n \"b\" : [ 2 , 3 ] }", -1);
    REQUIRE(r.ok);
    REQUIRE(r.text == R"({"a":1,"b":[2,3]})");
}

TEST_CASE("FormatJson reports parse errors", "[JsonLogic]") {
    const auto r = FormatJson("{ not valid }", 2);
    REQUIRE_FALSE(r.ok);
    REQUIRE_FALSE(r.error.empty());
    REQUIRE(r.text.empty());
}

TEST_CASE("FormatJson handles primitives and unicode", "[JsonLogic]") {
    REQUIRE(FormatJson("42", -1).text == "42");
    REQUIRE(FormatJson(R"("hi")", -1).text == R"("hi")");
    REQUIRE(FormatJson("true", -1).ok);
}
