// Security & privacy helpers. Pure-ish logic in superwin_core (unit-tested):
// cryptographically-secure random tokens (via CNG/BCrypt) and a password
// strength estimate based on character-set entropy.
#pragma once

#include <cstddef>
#include <string>

namespace superwin {

enum class TokenEncoding { Hex, Base64 };

// A cryptographically-secure random token of `numBytes` bytes, rendered as hex or
// Base64. Returns empty on failure or numBytes == 0.
std::string RandomToken(size_t numBytes, TokenEncoding encoding);

struct PasswordStrength {
    double      bits = 0;   // estimated entropy in bits
    int         score = 0;  // 0..4 bucket
    std::string label;      // "Very weak" .. "Very strong"
};

// Estimate strength from length and the character classes present (lower, upper,
// digits, symbols). Deterministic and unit-testable.
PasswordStrength EstimatePasswordStrength(const std::string& password);

}  // namespace superwin
