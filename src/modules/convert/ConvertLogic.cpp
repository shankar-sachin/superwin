#include "modules/convert/ConvertLogic.h"

#include <algorithm>
#include <cctype>
#include <iterator>

namespace superwin {
namespace {

struct LinearUnit { const char* name; double toBase; };

// Linear unit tables (factor to a category base unit).
const LinearUnit kLength[] = {
    {"Meters", 1.0}, {"Kilometers", 1000.0}, {"Centimeters", 0.01}, {"Millimeters", 0.001},
    {"Miles", 1609.344}, {"Yards", 0.9144}, {"Feet", 0.3048}, {"Inches", 0.0254},
};
const LinearUnit kMass[] = {
    {"Kilograms", 1.0}, {"Grams", 0.001}, {"Milligrams", 1e-6},
    {"Pounds", 0.45359237}, {"Ounces", 0.028349523125},
};
const LinearUnit kData[] = {
    {"Bytes", 1.0}, {"Kilobytes", 1024.0}, {"Megabytes", 1048576.0},
    {"Gigabytes", 1073741824.0}, {"Terabytes", 1099511627776.0},
};
const char* kTemp[] = {"Celsius", "Fahrenheit", "Kelvin"};

const LinearUnit* LinearTable(UnitCategory cat, size_t& count) {
    switch (cat) {
        case UnitCategory::Length:   count = std::size(kLength); return kLength;
        case UnitCategory::Mass:     count = std::size(kMass);   return kMass;
        case UnitCategory::DataSize: count = std::size(kData);   return kData;
        default:                     count = 0;                  return nullptr;
    }
}

std::optional<double> LinearFactor(UnitCategory cat, const std::string& unit) {
    size_t n = 0;
    const LinearUnit* t = LinearTable(cat, n);
    for (size_t i = 0; i < n; ++i) if (unit == t[i].name) return t[i].toBase;
    return std::nullopt;
}

double ToCelsius(const std::string& unit, double v) {
    if (unit == "Fahrenheit") return (v - 32.0) * 5.0 / 9.0;
    if (unit == "Kelvin")     return v - 273.15;
    return v;  // Celsius
}
double FromCelsius(const std::string& unit, double c) {
    if (unit == "Fahrenheit") return c * 9.0 / 5.0 + 32.0;
    if (unit == "Kelvin")     return c + 273.15;
    return c;
}

int DigitValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    return -1;
}

}  // namespace

std::vector<std::string> CategoryNames() {
    return {"Length", "Mass", "Temperature", "Data size"};
}

std::vector<std::string> UnitsFor(UnitCategory category) {
    if (category == UnitCategory::Temperature) {
        return {kTemp[0], kTemp[1], kTemp[2]};
    }
    size_t n = 0;
    const LinearUnit* t = LinearTable(category, n);
    std::vector<std::string> out;
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) out.emplace_back(t[i].name);
    return out;
}

std::optional<double> ConvertUnit(UnitCategory category, const std::string& fromUnit,
                                  const std::string& toUnit, double value) {
    if (category == UnitCategory::Temperature) {
        const auto valid = [](const std::string& u) {
            return u == kTemp[0] || u == kTemp[1] || u == kTemp[2];
        };
        if (!valid(fromUnit) || !valid(toUnit)) return std::nullopt;
        return FromCelsius(toUnit, ToCelsius(fromUnit, value));
    }
    auto from = LinearFactor(category, fromUnit);
    auto to = LinearFactor(category, toUnit);
    if (!from || !to) return std::nullopt;
    return value * (*from) / (*to);
}

std::optional<long long> ParseInBase(const std::string& text, int base) {
    if (base < 2 || base > 36) return std::nullopt;
    size_t i = 0;
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    bool negative = false;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) { negative = (text[i] == '-'); ++i; }
    if (i >= text.size()) return std::nullopt;

    long long value = 0;
    bool any = false;
    for (; i < text.size(); ++i) {
        if (std::isspace(static_cast<unsigned char>(text[i]))) break;
        const int d = DigitValue(text[i]);
        if (d < 0 || d >= base) return std::nullopt;
        value = value * base + d;
        any = true;
    }
    if (!any) return std::nullopt;
    return negative ? -value : value;
}

std::string FormatInBase(long long value, int base) {
    if (base < 2 || base > 36) return {};
    if (value == 0) return "0";
    static const char* digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const bool negative = value < 0;
    unsigned long long v = negative ? static_cast<unsigned long long>(-(value + 1)) + 1ull
                                    : static_cast<unsigned long long>(value);
    std::string out;
    while (v > 0) { out.push_back(digits[v % base]); v /= base; }
    if (negative) out.push_back('-');
    std::reverse(out.begin(), out.end());
    return out;
}

}  // namespace superwin
