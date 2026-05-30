#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "modules/volume/VolumeMath.h"

using namespace superwin;
using Catch::Approx;

TEST_CASE("scalar <-> percent round-trips", "[volume]") {
    REQUIRE(PercentFromScalar(0.0f) == 0);
    REQUIRE(PercentFromScalar(1.0f) == 100);
    REQUIRE(PercentFromScalar(0.5f) == 50);

    REQUIRE(ScalarFromPercent(0) == Approx(0.0f));
    REQUIRE(ScalarFromPercent(100) == Approx(1.0f));
    REQUIRE(ScalarFromPercent(25) == Approx(0.25f));
}

TEST_CASE("percent and scalar clamp out-of-range input", "[volume]") {
    REQUIRE(PercentFromScalar(2.0f) == 100);
    REQUIRE(PercentFromScalar(-1.0f) == 0);
    REQUIRE(ScalarFromPercent(150) == Approx(1.0f));
    REQUIRE(ScalarFromPercent(-10) == Approx(0.0f));
}

TEST_CASE("dB conversions follow 20*log10", "[volume]") {
    REQUIRE(DbFromScalar(1.0f) == Approx(0.0f));
    REQUIRE(DbFromScalar(0.5f) == Approx(-6.0206f).margin(0.01));
    REQUIRE(DbFromScalar(0.1f) == Approx(-20.0f).margin(0.01));
}

TEST_CASE("silence maps to the floor and back to zero", "[volume]") {
    REQUIRE(DbFromScalar(0.0f) == Approx(kSilenceDb));
    REQUIRE(ScalarFromDb(kSilenceDb) == Approx(0.0f));
    REQUIRE(ScalarFromDb(kSilenceDb - 10.0f) == Approx(0.0f));
}

TEST_CASE("dB -> scalar -> dB round-trips above silence", "[volume]") {
    for (float db : {-3.0f, -12.0f, -30.0f}) {
        REQUIRE(DbFromScalar(ScalarFromDb(db)) == Approx(db).margin(0.01));
    }
}

TEST_CASE("FormatDb renders silence as -inf", "[volume]") {
    REQUIRE(FormatDb(kSilenceDb) == "-\xE2\x88\x9E dB");
    REQUIRE(FormatDb(0.0f) == "+0.0 dB");
    REQUIRE(FormatDb(-12.3f) == "-12.3 dB");
}
