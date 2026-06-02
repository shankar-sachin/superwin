#include "modules/text/TextLogic.h"

#include <array>
#include <cctype>

namespace superwin {
namespace {

unsigned char Lower(unsigned char c) {
    return static_cast<unsigned char>(std::tolower(c));
}
unsigned char Upper(unsigned char c) {
    return static_cast<unsigned char>(std::toupper(c));
}

constexpr const char* kB64 =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Maps a Base64 character to its 6-bit value, or 0xFF if not in the alphabet.
int B64Value(char c) {
    for (int i = 0; i < 64; ++i) {
        if (kB64[i] == c) return i;
    }
    return -1;
}

}  // namespace

std::string ToUpperAscii(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = static_cast<char>(Upper(static_cast<unsigned char>(c)));
    return out;
}

std::string ToLowerAscii(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = static_cast<char>(Lower(static_cast<unsigned char>(c)));
    return out;
}

std::string ToTitleCase(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool startOfWord = true;
    for (unsigned char c : s) {
        if (std::isspace(c)) {
            startOfWord = true;
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back(static_cast<char>(startOfWord ? Upper(c) : Lower(c)));
            startOfWord = false;
        }
    }
    return out;
}

std::string Base64Encode(const std::string& data) {
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 3 <= data.size()) {
        const unsigned n = (static_cast<unsigned char>(data[i]) << 16) |
                           (static_cast<unsigned char>(data[i + 1]) << 8) |
                           static_cast<unsigned char>(data[i + 2]);
        out.push_back(kB64[(n >> 18) & 0x3F]);
        out.push_back(kB64[(n >> 12) & 0x3F]);
        out.push_back(kB64[(n >> 6) & 0x3F]);
        out.push_back(kB64[n & 0x3F]);
        i += 3;
    }
    const size_t rem = data.size() - i;
    if (rem == 1) {
        const unsigned n = static_cast<unsigned char>(data[i]) << 16;
        out.push_back(kB64[(n >> 18) & 0x3F]);
        out.push_back(kB64[(n >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        const unsigned n = (static_cast<unsigned char>(data[i]) << 16) |
                           (static_cast<unsigned char>(data[i + 1]) << 8);
        out.push_back(kB64[(n >> 18) & 0x3F]);
        out.push_back(kB64[(n >> 12) & 0x3F]);
        out.push_back(kB64[(n >> 6) & 0x3F]);
        out.push_back('=');
    }
    return out;
}

std::optional<std::string> Base64Decode(const std::string& text) {
    if (text.size() % 4 != 0) return std::nullopt;
    std::string out;
    out.reserve((text.size() / 4) * 3);
    for (size_t i = 0; i < text.size(); i += 4) {
        int v[4];
        int pad = 0;
        for (int j = 0; j < 4; ++j) {
            const char c = text[i + j];
            if (c == '=') {
                // Padding is only valid in the last group, trailing positions.
                if (i + 4 != text.size() || j < 2) return std::nullopt;
                v[j] = 0;
                ++pad;
            } else {
                const int val = B64Value(c);
                if (val < 0 || pad > 0) return std::nullopt;  // junk after padding
                v[j] = val;
            }
        }
        const unsigned n = (v[0] << 18) | (v[1] << 12) | (v[2] << 6) | v[3];
        out.push_back(static_cast<char>((n >> 16) & 0xFF));
        if (pad < 2) out.push_back(static_cast<char>((n >> 8) & 0xFF));
        if (pad < 1) out.push_back(static_cast<char>(n & 0xFF));
    }
    return out;
}

TextStats Analyze(const std::string& s) {
    TextStats st;
    st.characters = s.size();
    bool inWord = false;
    for (unsigned char c : s) {
        if (std::isspace(c)) {
            inWord = false;
        } else if (!inWord) {
            inWord = true;
            ++st.words;
        }
    }
    if (!s.empty()) {
        st.lines = 1;
        for (char c : s) {
            if (c == '\n') ++st.lines;
        }
    }
    return st;
}

}  // namespace superwin
