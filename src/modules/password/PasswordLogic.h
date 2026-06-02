// Password generation. Pure logic in superwin_core (unit-tested), so the
// charset composition, length guarantees and strength estimate can be verified
// without standing up any UI.
#pragma once

#include <cstdint>
#include <string>

namespace superwin {

// Which character classes a generated password may draw from. At least one
// class must be enabled for GeneratePassword to produce output.
struct PasswordOptions {
    int length = 16;
    bool lowercase = true;
    bool uppercase = true;
    bool digits = true;
    bool symbols = false;
    // Drop visually ambiguous characters (O/0, l/1/I, etc.) from the pool.
    bool avoidAmbiguous = false;
};

// The full pool of characters the options select, after ambiguity filtering.
// Empty if no class is enabled.
std::string BuildCharset(const PasswordOptions& opts);

// Generate a password of opts.length characters drawn from BuildCharset(opts).
// `seed` makes generation deterministic (used by tests); callers that want a
// fresh password each time pass a random seed. When more than one class is
// enabled and length permits, the result is guaranteed to contain at least one
// character from every enabled class. Returns "" if no class is enabled or
// length <= 0.
std::string GeneratePassword(const PasswordOptions& opts, uint32_t seed);

// Rough strength in bits of entropy: length * log2(charsetSize), floored.
// 0 if the charset is empty.
int EstimateStrengthBits(const PasswordOptions& opts);

}  // namespace superwin
