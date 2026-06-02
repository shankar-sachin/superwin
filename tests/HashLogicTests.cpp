#include <catch2/catch_test_macros.hpp>

#include "modules/hash/HashLogic.h"

using namespace superwin;

// Known answers for the empty string and "abc".
TEST_CASE("HashBytes matches known vectors", "[hash]") {
    CHECK(HashBytes(HashAlgo::Md5, "") == "d41d8cd98f00b204e9800998ecf8427e");
    CHECK(HashBytes(HashAlgo::Sha1, "") == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    CHECK(HashBytes(HashAlgo::Sha256, "") ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    CHECK(HashBytes(HashAlgo::Md5, "abc") == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(HashBytes(HashAlgo::Sha1, "abc") == "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK(HashBytes(HashAlgo::Sha256, "abc") ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST_CASE("HashAlgoName is stable", "[hash]") {
    CHECK(std::string(HashAlgoName(HashAlgo::Md5)) == "MD5");
    CHECK(std::string(HashAlgoName(HashAlgo::Sha1)) == "SHA-1");
    CHECK(std::string(HashAlgoName(HashAlgo::Sha256)) == "SHA-256");
}
