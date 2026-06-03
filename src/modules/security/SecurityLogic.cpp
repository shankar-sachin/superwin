#include "modules/security/SecurityLogic.h"

#include <Windows.h>
#include <bcrypt.h>

#include <cctype>
#include <cmath>
#include <vector>

#include "modules/text/TextLogic.h"  // Base64Encode

namespace superwin {
namespace {

std::string ToHex(const std::vector<uint8_t>& bytes) {
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (uint8_t b : bytes) {
        out.push_back(kHex[b >> 4]);
        out.push_back(kHex[b & 0x0F]);
    }
    return out;
}

}  // namespace

std::string RandomToken(size_t numBytes, TokenEncoding encoding) {
    if (numBytes == 0) return {};
    std::vector<uint8_t> buf(numBytes);
    const NTSTATUS st = ::BCryptGenRandom(nullptr, buf.data(), static_cast<ULONG>(buf.size()),
                                          BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (st != 0) return {};  // STATUS_SUCCESS == 0
    if (encoding == TokenEncoding::Hex) return ToHex(buf);
    return Base64Encode(std::string(buf.begin(), buf.end()));
}

PasswordStrength EstimatePasswordStrength(const std::string& password) {
    PasswordStrength s;
    if (password.empty()) { s.label = "Empty"; return s; }

    bool lower = false, upper = false, digit = false, symbol = false;
    for (unsigned char c : password) {
        if (c >= 'a' && c <= 'z') lower = true;
        else if (c >= 'A' && c <= 'Z') upper = true;
        else if (c >= '0' && c <= '9') digit = true;
        else symbol = true;
    }
    int pool = 0;
    if (lower) pool += 26;
    if (upper) pool += 26;
    if (digit) pool += 10;
    if (symbol) pool += 33;  // printable ASCII punctuation
    if (pool == 0) pool = 1;

    s.bits = static_cast<double>(password.size()) * std::log2(static_cast<double>(pool));

    if (s.bits < 28)       { s.score = 0; s.label = "Very weak"; }
    else if (s.bits < 36)  { s.score = 1; s.label = "Weak"; }
    else if (s.bits < 60)  { s.score = 2; s.label = "Fair"; }
    else if (s.bits < 128) { s.score = 3; s.label = "Strong"; }
    else                   { s.score = 4; s.label = "Very strong"; }
    return s;
}

}  // namespace superwin
