#include "modules/password/PasswordLogic.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace superwin {
namespace {

constexpr const char* kLower = "abcdefghijklmnopqrstuvwxyz";
constexpr const char* kUpper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr const char* kDigits = "0123456789";
constexpr const char* kSymbols = "!@#$%^&*()-_=+[]{};:,.?/";
// Characters that are easy to confuse with one another in most fonts.
constexpr const char* kAmbiguous = "O0oIl1|S5B8Z2";

std::string Filter(std::string pool, bool avoidAmbiguous) {
    if (!avoidAmbiguous) return pool;
    std::string out;
    for (char c : pool) {
        if (std::string(kAmbiguous).find(c) == std::string::npos) out.push_back(c);
    }
    return out;
}

// The per-class pools the options enable, after ambiguity filtering. Empty
// pools (e.g. a class fully removed by filtering) are dropped.
std::vector<std::string> EnabledClasses(const PasswordOptions& opts) {
    std::vector<std::string> classes;
    auto add = [&](bool on, const char* pool) {
        if (!on) return;
        std::string p = Filter(pool, opts.avoidAmbiguous);
        if (!p.empty()) classes.push_back(std::move(p));
    };
    add(opts.lowercase, kLower);
    add(opts.uppercase, kUpper);
    add(opts.digits, kDigits);
    add(opts.symbols, kSymbols);
    return classes;
}

}  // namespace

std::string BuildCharset(const PasswordOptions& opts) {
    std::string all;
    for (const auto& cls : EnabledClasses(opts)) all += cls;
    return all;
}

std::string GeneratePassword(const PasswordOptions& opts, uint32_t seed) {
    const auto classes = EnabledClasses(opts);
    if (classes.empty() || opts.length <= 0) return {};

    std::string pool = BuildCharset(opts);
    std::mt19937 rng(seed);
    auto pick = [&](const std::string& s) {
        std::uniform_int_distribution<size_t> dist(0, s.size() - 1);
        return s[dist(rng)];
    };

    std::string out;
    out.reserve(static_cast<size_t>(opts.length));

    // Guarantee one character from each enabled class, as long as length allows.
    for (const auto& cls : classes) {
        if (static_cast<int>(out.size()) >= opts.length) break;
        out.push_back(pick(cls));
    }
    // Fill the remainder from the combined pool.
    while (static_cast<int>(out.size()) < opts.length) out.push_back(pick(pool));

    // Shuffle so the guaranteed characters aren't stuck at the front.
    std::shuffle(out.begin(), out.end(), rng);
    return out;
}

int EstimateStrengthBits(const PasswordOptions& opts) {
    const std::string pool = BuildCharset(opts);
    if (pool.empty() || opts.length <= 0) return 0;
    const double bits = opts.length * std::log2(static_cast<double>(pool.size()));
    return static_cast<int>(bits);
}

}  // namespace superwin
