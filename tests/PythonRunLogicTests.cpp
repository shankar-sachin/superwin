#include <catch2/catch_test_macros.hpp>

#include "modules/python/PythonRunLogic.h"

using namespace superwin;

TEST_CASE("Interpreter candidates prefer the py launcher", "[python]") {
    auto c = DefaultInterpreterCandidates();
    REQUIRE_FALSE(c.empty());
    REQUIRE(c.front() == L"py.exe");
}

TEST_CASE("QuoteArg only quotes when needed", "[python]") {
    REQUIRE(QuoteArg(L"python.exe") == L"python.exe");
    REQUIRE(QuoteArg(L"C:\\Program Files\\Python\\python.exe") ==
            L"\"C:\\Program Files\\Python\\python.exe\"");
    // Embedded quote is escaped.
    REQUIRE(QuoteArg(L"a\"b") == L"\"a\\\"b\"");
    // Trailing backslash before the closing quote is doubled.
    REQUIRE(QuoteArg(L"C:\\dir with space\\") == L"\"C:\\dir with space\\\\\"");
}

TEST_CASE("BuildPythonCommandLine passes -u and quotes paths", "[python]") {
    auto cmd = BuildPythonCommandLine(L"py.exe", L"C:\\tmp\\a b.py");
    REQUIRE(cmd == L"py.exe -u \"C:\\tmp\\a b.py\"");
}
