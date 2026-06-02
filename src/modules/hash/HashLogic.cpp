#include "modules/hash/HashLogic.h"

#include <Windows.h>
#include <bcrypt.h>

#include <array>
#include <fstream>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace superwin {
namespace {

const wchar_t* AlgoId(HashAlgo algo) {
    switch (algo) {
        case HashAlgo::Md5:    return BCRYPT_MD5_ALGORITHM;
        case HashAlgo::Sha1:   return BCRYPT_SHA1_ALGORITHM;
        case HashAlgo::Sha256: return BCRYPT_SHA256_ALGORITHM;
    }
    return BCRYPT_SHA256_ALGORITHM;
}

std::string ToHex(const std::vector<unsigned char>& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char b : bytes) {
        out.push_back(digits[b >> 4]);
        out.push_back(digits[b & 0x0F]);
    }
    return out;
}

// Streams `feed` into a fresh hash object and returns the lowercase hex digest.
// `feed` is invoked with the BCRYPT_HASH_HANDLE so callers can push data in
// chunks (in-memory string or file blocks) without duplicating CNG setup.
template <typename Feed>
std::string Digest(HashAlgo algo, Feed&& feed) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, AlgoId(algo), nullptr, 0) != 0) return {};

    DWORD hashLen = 0, cb = 0;
    if (BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLen),
                          sizeof(hashLen), &cb, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return {};
    }

    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return {};
    }

    bool ok = feed(hash);

    std::string result;
    if (ok) {
        std::vector<unsigned char> out(hashLen);
        if (BCryptFinishHash(hash, out.data(), hashLen, 0) == 0) result = ToHex(out);
    }

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    return result;
}

}  // namespace

std::string HashBytes(HashAlgo algo, const std::string& data) {
    return Digest(algo, [&](BCRYPT_HASH_HANDLE hash) {
        if (data.empty()) return true;  // valid: hash of empty input
        return BCryptHashData(hash,
            reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),
            static_cast<ULONG>(data.size()), 0) == 0;
    });
}

std::optional<std::string> HashFile(HashAlgo algo, const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;

    std::string digest = Digest(algo, [&](BCRYPT_HASH_HANDLE hash) {
        std::array<char, 64 * 1024> buf;
        while (in) {
            in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
            const std::streamsize got = in.gcount();
            if (got > 0 &&
                BCryptHashData(hash, reinterpret_cast<PUCHAR>(buf.data()),
                               static_cast<ULONG>(got), 0) != 0) {
                return false;
            }
        }
        return true;
    });
    if (digest.empty()) return std::nullopt;
    return digest;
}

const char* HashAlgoName(HashAlgo algo) {
    switch (algo) {
        case HashAlgo::Md5:    return "MD5";
        case HashAlgo::Sha1:   return "SHA-1";
        case HashAlgo::Sha256: return "SHA-256";
    }
    return "SHA-256";
}

}  // namespace superwin
