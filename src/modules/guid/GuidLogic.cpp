#include "modules/guid/GuidLogic.h"

#include <random>

namespace superwin {
namespace {

const char* HexDigits(bool upper) { return upper ? "0123456789ABCDEF" : "0123456789abcdef"; }

}  // namespace

std::string FormatGuid(const GuidBytes& bytes, const GuidOptions& opt) {
    const char* hex = HexDigits(opt.uppercase);
    std::string out;
    out.reserve(38);
    if (opt.braces) out.push_back('{');
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (opt.hyphens && (i == 4 || i == 6 || i == 8 || i == 10)) out.push_back('-');
        out.push_back(hex[bytes[i] >> 4]);
        out.push_back(hex[bytes[i] & 0x0F]);
    }
    if (opt.braces) out.push_back('}');
    return out;
}

GuidBytes RandomGuidBytes() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned> dist(0, 255);

    GuidBytes b{};
    for (auto& byte : b) byte = static_cast<uint8_t>(dist(gen));

    // Version 4 (random): high nibble of byte 6 = 0100.
    b[6] = static_cast<uint8_t>((b[6] & 0x0F) | 0x40);
    // RFC 4122 variant: top two bits of byte 8 = 10.
    b[8] = static_cast<uint8_t>((b[8] & 0x3F) | 0x80);
    return b;
}

std::string NewGuid(const GuidOptions& opt) {
    return FormatGuid(RandomGuidBytes(), opt);
}

}  // namespace superwin
