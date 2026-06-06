#include "modules/graph/Latex.h"

#include <cctype>
#include <cstddef>
#include <set>
#include <string>

namespace superwin {
namespace {

std::string Convert(const std::string& s, size_t& i);  // until '}' or end
std::string Command(const std::string& s, size_t& i);

void SkipWs(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '~' || s[i] == '\t')) ++i;
}

std::string ReadName(const std::string& s, size_t& i) {
    std::string n;
    while (i < s.size() && std::isalpha(static_cast<unsigned char>(s[i]))) n += s[i++];
    return n;
}

// The next single argument (a {group}, a \command, or one character), converted.
std::string ArgRaw(const std::string& s, size_t& i) {
    SkipWs(s, i);
    if (i >= s.size()) return "";
    if (s[i] == '{') { ++i; std::string r = Convert(s, i); if (i < s.size() && s[i] == '}') ++i; return r; }
    if (s[i] == '\\') return Command(s, i);
    return std::string(1, s[i++]);
}

// Read optional _sub / ^sup limits (either order) for \sum \prod \int.
void ReadLimits(const std::string& s, size_t& i, std::string& sub, std::string& sup) {
    for (int k = 0; k < 2; ++k) {
        SkipWs(s, i);
        if (i < s.size() && s[i] == '_') { ++i; sub = ArgRaw(s, i); }
        else if (i < s.size() && s[i] == '^') { ++i; sup = ArgRaw(s, i); }
        else break;
    }
}

std::string Series(const std::string& s, size_t& i, bool isSum) {
    std::string sub, sup;
    ReadLimits(s, i, sub, sup);
    std::string lo = sub;
    const auto eq = sub.find('=');
    if (eq != std::string::npos) lo = sub.substr(eq + 1);  // "n=1" -> "1"
    std::string body = Convert(s, i);  // the summand: rest of this group
    return std::string(isSum ? "sum(" : "prod(") + body + ", " + lo + ", " + sup + ")";
}

std::string Integral(const std::string& s, size_t& i) {
    std::string sub, sup;
    ReadLimits(s, i, sub, sup);
    std::string body = Convert(s, i);  // integrand, ending in a differential
    // Strip a trailing differential ("dx", possibly with stray operators/spaces).
    const auto p = body.rfind("dx");
    if (p != std::string::npos && p + 2 >= body.size()) body = body.substr(0, p);
    while (!body.empty() && (body.back() == ' ' || body.back() == '*' || body.back() == '.')) body.pop_back();
    return "int(" + body + ")";
}

std::string Command(const std::string& s, size_t& i) {
    ++i;  // skip backslash
    if (i < s.size() && !std::isalpha(static_cast<unsigned char>(s[i]))) {
        const char c = s[i++];
        if (c == '(' || c == ')') return std::string(1, c);
        return "";  // \, \; \! \  etc. -> nothing
    }
    const std::string name = ReadName(s, i);

    if (name == "frac" || name == "dfrac" || name == "tfrac") {
        const std::string a = ArgRaw(s, i), b = ArgRaw(s, i);
        return "((" + a + ")/(" + b + "))";
    }
    if (name == "sqrt") {
        SkipWs(s, i);
        if (i < s.size() && s[i] == '[') {
            ++i;
            std::string root;
            while (i < s.size() && s[i] != ']') root += s[i++];
            if (i < s.size() && s[i] == ']') ++i;
            const std::string a = ArgRaw(s, i);
            return "((" + a + ")^(1/(" + root + ")))";
        }
        return "sqrt(" + ArgRaw(s, i) + ")";
    }
    if (name == "cdot" || name == "times") return "*";
    if (name == "div") return "/";
    if (name == "pi") return "pi";
    if (name == "exponentialE") return "e";
    if (name == "differentialD") return "d";
    if (name == "left" || name == "right") {
        SkipWs(s, i);
        if (i < s.size()) {
            const char d = s[i];
            if (d == '(' || d == ')') { ++i; return std::string(1, d); }
            if (d == '[') { ++i; return "("; }
            if (d == ']') { ++i; return ")"; }
            if (d == '.' || d == '|') { ++i; return ""; }
        }
        return "";
    }
    if (name == "operatorname" || name == "mathrm" || name == "mathit" || name == "text")
        return ArgRaw(s, i);
    if (name == "sum" || name == "prod") return Series(s, i, name == "sum");
    if (name == "int") return Integral(s, i);

    static const std::set<std::string> kFns = {
        "sin", "cos", "tan", "cot", "sec", "csc", "sinh", "cosh", "tanh", "ln", "log", "exp", "abs"};
    if (kFns.count(name)) return name;
    if (name == "arcsin") return "asin";
    if (name == "arccos") return "acos";
    if (name == "arctan") return "atan";
    // Unknown command (a Greek letter etc.) -> drop it rather than break parsing.
    return "";
}

std::string Convert(const std::string& s, size_t& i) {
    std::string out;
    while (i < s.size() && s[i] != '}') {
        const char c = s[i];
        if (c == ' ' || c == '~' || c == '\t') { ++i; continue; }
        if (c == '{') { ++i; out += '(' + Convert(s, i) + ')'; if (i < s.size() && s[i] == '}') ++i; continue; }
        if (c == '^') { ++i; out += "^(" + ArgRaw(s, i) + ")"; continue; }
        if (c == '_') { ++i; ArgRaw(s, i); continue; }  // drop stray subscripts
        if (c == '\\') { out += Command(s, i); continue; }
        if (c == '|') { ++i; continue; }  // drop abs bars (unsupported)
        out += c; ++i;
    }
    return out;
}

}  // namespace

std::string LatexToInfix(const std::string& latex) {
    size_t i = 0;
    return Convert(latex, i);
}

}  // namespace superwin
