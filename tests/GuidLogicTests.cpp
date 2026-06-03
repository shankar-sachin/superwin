#include <catch2/catch_test_macros.hpp>

#include "modules/guid/GuidLogic.h"

using namespace superwin;

namespace {
// 00 01 02 ... 0F — a predictable pattern for formatting checks.
GuidBytes Sequential() {
    GuidBytes b{};
    for (uint8_t i = 0; i < 16; ++i) b[i] = i;
    return b;
}
}  // namespace

TEST_CASE("FormatGuid default is lowercase, hyphenated, no braces", "[GuidLogic]") {
    GuidOptions opt;  // defaults
    REQUIRE(FormatGuid(Sequential(), opt) == "00010203-0405-0607-0809-0a0b0c0d0e0f");
}

TEST_CASE("FormatGuid honours uppercase and braces", "[GuidLogic]") {
    GuidOptions opt;
    opt.uppercase = true;
    opt.braces = true;
    REQUIRE(FormatGuid(Sequential(), opt) == "{00010203-0405-0607-0809-0A0B0C0D0E0F}");
}

TEST_CASE("FormatGuid without hyphens is 32 hex chars", "[GuidLogic]") {
    GuidOptions opt;
    opt.hyphens = false;
    const auto s = FormatGuid(Sequential(), opt);
    REQUIRE(s == "000102030405060708090a0b0c0d0e0f");
    REQUIRE(s.size() == 32);
}

TEST_CASE("RandomGuidBytes sets version 4 and RFC 4122 variant", "[GuidLogic]") {
    for (int i = 0; i < 50; ++i) {
        const auto b = RandomGuidBytes();
        REQUIRE((b[6] & 0xF0) == 0x40);  // version nibble == 4
        REQUIRE((b[8] & 0xC0) == 0x80);  // variant bits == 10
    }
}

TEST_CASE("NewGuid produces a well-formed v4 string", "[GuidLogic]") {
    const std::string g = NewGuid(GuidOptions{});
    REQUIRE(g.size() == 36);     // 32 hex + 4 hyphens
    REQUIRE(g[14] == '4');       // version marker
    REQUIRE(g[8] == '-');
}
