#include <catch2/catch_test_macros.hpp>

#include <string>

#include "modules/text/TextLogic.h"

using namespace superwin;

TEST_CASE("Case transforms", "[text]") {
    CHECK(ToUpperAscii("Hello, World!") == "HELLO, WORLD!");
    CHECK(ToLowerAscii("Hello, World!") == "hello, world!");
    CHECK(ToTitleCase("the quick brown fox") == "The Quick Brown Fox");
    CHECK(ToTitleCase("aLREADY mIxEd") == "Already Mixed");
}

TEST_CASE("Base64 encode matches known vectors", "[text]") {
    CHECK(Base64Encode("") == "");
    CHECK(Base64Encode("f") == "Zg==");
    CHECK(Base64Encode("fo") == "Zm8=");
    CHECK(Base64Encode("foo") == "Zm9v");
    CHECK(Base64Encode("foob") == "Zm9vYg==");
    CHECK(Base64Encode("fooba") == "Zm9vYmE=");
    CHECK(Base64Encode("foobar") == "Zm9vYmFy");
}

TEST_CASE("Base64 round-trips arbitrary bytes", "[text]") {
    const std::string data = "The quick brown fox jumps over 13 lazy dogs.";
    auto decoded = Base64Decode(Base64Encode(data));
    REQUIRE(decoded.has_value());
    CHECK(*decoded == data);
}

TEST_CASE("Base64 decode rejects malformed input", "[text]") {
    CHECK_FALSE(Base64Decode("Zg=").has_value());     // bad length
    CHECK_FALSE(Base64Decode("Zg=A").has_value());    // data after padding
    CHECK_FALSE(Base64Decode("****").has_value());    // outside alphabet
    CHECK(Base64Decode("").has_value());              // empty is valid
}

TEST_CASE("Analyze counts characters, words and lines", "[text]") {
    auto a = Analyze("");
    CHECK(a.characters == 0);
    CHECK(a.words == 0);
    CHECK(a.lines == 0);

    auto b = Analyze("hello world");
    CHECK(b.characters == 11);
    CHECK(b.words == 2);
    CHECK(b.lines == 1);

    auto c = Analyze("one\ntwo  three\n");
    CHECK(c.words == 3);
    CHECK(c.lines == 3);  // "one", "two  three", trailing empty
}
