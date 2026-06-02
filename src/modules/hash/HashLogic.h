// Cryptographic digests over text or files, via the Windows CNG (BCrypt) API.
// Pure logic in superwin_core so it can be unit-tested without any UI.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace superwin {

enum class HashAlgo { Md5, Sha1, Sha256 };

// Lowercase hex digest of `data` (treated as raw bytes). Empty on failure.
std::string HashBytes(HashAlgo algo, const std::string& data);

// Lowercase hex digest of a file's contents, or nullopt if it can't be read.
std::optional<std::string> HashFile(HashAlgo algo, const std::filesystem::path& path);

// Human-readable algorithm name ("MD5", "SHA-1", "SHA-256").
const char* HashAlgoName(HashAlgo algo);

}  // namespace superwin
