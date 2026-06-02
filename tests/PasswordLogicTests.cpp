#include <catch2/catch_test_macros.hpp>

#include <string>

#include "modules/password/PasswordLogic.h"

using namespace superwin;

namespace {
bool AllFromPool(const std::string& pw, const std::string& pool) {
    for (char c : pw) {
        if (pool.find(c) == std::string::npos) return false;
    }
    return true;
}
bool HasAnyOf(const std::string& pw, const std::string& set) {
    for (char c : pw) {
        if (set.find(c) != std::string::npos) return true;
    }
    return false;
}
}  // namespace

TEST_CASE("Charset reflects enabled classes", "[password]") {
    PasswordOptions o;
    o.lowercase = true; o.uppercase = false; o.digits = false; o.symbols = false;
    CHECK(BuildCharset(o) == "abcdefghijklmnopqrstuvwxyz");

    o.digits = true;
    CHECK(BuildCharset(o) == "abcdefghijklmnopqrstuvwxyz0123456789");

    PasswordOptions none;
    none.lowercase = none.uppercase = none.digits = none.symbols = false;
    CHECK(BuildCharset(none).empty());
}

TEST_CASE("Generated password has requested length and only pool chars", "[password]") {
    PasswordOptions o;
    o.length = 20;
    const std::string pool = BuildCharset(o);
    const std::string pw = GeneratePassword(o, 12345);
    REQUIRE(pw.size() == 20);
    CHECK(AllFromPool(pw, pool));
}

TEST_CASE("Generation is deterministic for a fixed seed", "[password]") {
    PasswordOptions o;
    o.length = 24;
    CHECK(GeneratePassword(o, 42) == GeneratePassword(o, 42));
    CHECK(GeneratePassword(o, 1) != GeneratePassword(o, 2));
}

TEST_CASE("Every enabled class appears when length allows", "[password]") {
    PasswordOptions o;
    o.length = 16;
    o.lowercase = o.uppercase = o.digits = o.symbols = true;
    const std::string pw = GeneratePassword(o, 777);
    CHECK(HasAnyOf(pw, "abcdefghijklmnopqrstuvwxyz"));
    CHECK(HasAnyOf(pw, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    CHECK(HasAnyOf(pw, "0123456789"));
    CHECK(HasAnyOf(pw, "!@#$%^&*()-_=+[]{};:,.?/"));
}

TEST_CASE("Avoid-ambiguous removes confusable characters", "[password]") {
    PasswordOptions o;
    o.length = 200;  // large enough to very likely hit every pool char
    o.avoidAmbiguous = true;
    const std::string pw = GeneratePassword(o, 9);
    CHECK_FALSE(HasAnyOf(pw, "O0Il1"));
}

TEST_CASE("No class selected yields empty password", "[password]") {
    PasswordOptions o;
    o.lowercase = o.uppercase = o.digits = o.symbols = false;
    CHECK(GeneratePassword(o, 1).empty());
}

TEST_CASE("Strength grows with length and pool size", "[password]") {
    PasswordOptions small;
    small.length = 8; small.lowercase = true;
    small.uppercase = small.digits = small.symbols = false;

    PasswordOptions big = small;
    big.length = 16;
    CHECK(EstimateStrengthBits(big) > EstimateStrengthBits(small));

    PasswordOptions wider = small;
    wider.uppercase = wider.digits = true;
    CHECK(EstimateStrengthBits(wider) > EstimateStrengthBits(small));
}
