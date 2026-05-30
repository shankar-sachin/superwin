#include "modules/volume/VolumeMath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace superwin {

float ClampScalar(float scalar) {
    return std::clamp(scalar, 0.0f, 1.0f);
}

int ClampPercent(int percent) {
    return std::clamp(percent, 0, 100);
}

int PercentFromScalar(float scalar) {
    return static_cast<int>(std::lround(ClampScalar(scalar) * 100.0f));
}

float ScalarFromPercent(int percent) {
    return ClampPercent(percent) / 100.0f;
}

float DbFromScalar(float scalar) {
    scalar = ClampScalar(scalar);
    if (scalar <= 0.0f) return kSilenceDb;
    const float db = 20.0f * std::log10(scalar);
    return std::max(db, kSilenceDb);
}

float ScalarFromDb(float db) {
    if (db <= kSilenceDb) return 0.0f;
    return ClampScalar(std::pow(10.0f, db / 20.0f));
}

std::string FormatDb(float db) {
    if (db <= kSilenceDb) return "-\xE2\x88\x9E dB";  // "-∞ dB" (UTF-8)
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%+.1f dB", db);
    return buf;
}

}  // namespace superwin
