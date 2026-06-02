// Text utilities: case transforms, Base64, and simple counts. Pure logic in
// superwin_core (unit-tested). ASCII-oriented transforms operate per byte, so
// they pass UTF-8 multibyte sequences through unchanged.
#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace superwin {

std::string ToUpperAscii(const std::string& s);
std::string ToLowerAscii(const std::string& s);
// Capitalise the first letter of each whitespace-separated word, lowercasing
// the rest.
std::string ToTitleCase(const std::string& s);

// Base64 (standard alphabet, '=' padding).
std::string Base64Encode(const std::string& data);
// nullopt if `text` contains characters outside the Base64 alphabet or is
// malformed (bad length/padding).
std::optional<std::string> Base64Decode(const std::string& text);

struct TextStats {
    size_t characters = 0;  // bytes, excluding nothing
    size_t words = 0;       // whitespace-separated runs
    size_t lines = 0;       // 0 for empty input, else newline count + 1
};
TextStats Analyze(const std::string& s);

}  // namespace superwin
