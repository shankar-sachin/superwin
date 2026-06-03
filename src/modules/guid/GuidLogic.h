// GUID/UUID generator. Pure logic in superwin_core (unit-tested). Produces
// random version-4 UUIDs and formats them with the usual display options.
#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace superwin {

struct GuidOptions {
    bool uppercase = false;  // hex digits A-F vs a-f
    bool hyphens = true;     // 8-4-4-4-12 grouping vs a plain 32-char string
    bool braces = false;     // wrap in { }
};

using GuidBytes = std::array<uint8_t, 16>;

// Format a raw 16-byte UUID per the options.
std::string FormatGuid(const GuidBytes& bytes, const GuidOptions& opt);

// 16 random bytes with the version (4) and variant (RFC 4122) bits set.
GuidBytes RandomGuidBytes();

// Convenience: a freshly generated, formatted version-4 GUID.
std::string NewGuid(const GuidOptions& opt);

}  // namespace superwin
