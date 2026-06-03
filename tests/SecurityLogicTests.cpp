#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cctype>

#include "modules/security/SecurityLogic.h"

using namespace superwin;

TEST_CASE("RandomToken hex has the right length and alphabet", "[SecurityLogic]") {
    const std::string t = RandomToken(16, TokenEncoding::Hex);
    REQUIRE(t.size() == 32);  // 2 hex chars per byte
    REQUIRE(std::all_of(t.begin(), t.end(), [](char c) {
        return std::isxdigit(static_cast<unsigned char>(c)) != 0;
    }));
}

TEST_CASE("RandomToken base64 length matches byte count", "[SecurityLogic]") {
    const std::string t = RandomToken(15, TokenEncoding::Base64);
    REQUIRE(t.size() == 20);  // ceil(15/3)*4
}

TEST_CASE("RandomToken of zero bytes is empty", "[SecurityLogic]") {
    REQUIRE(RandomToken(0, TokenEncoding::Hex).empty());
}

TEST_CASE("RandomToken is (almost certainly) not constant", "[SecurityLogic]") {
    REQUIRE(RandomToken(32, TokenEncoding::Hex) != RandomToken(32, TokenEncoding::Hex));
}

TEST_CASE("Password strength scales with length and variety", "[SecurityLogic]") {
    REQUIRE(EstimatePasswordStrength("").score == 0);
    const auto weak = EstimatePasswordStrength("abc");
    const auto strong = EstimatePasswordStrength("Tr0ub4dour&3xtra-Long!Pass");
    REQUIRE(weak.score < strong.score);
    REQUIRE(strong.bits > weak.bits);
    REQUIRE(strong.score >= 3);
}
