// Unit and number-base conversions. Pure logic in superwin_core (unit-tested).
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace superwin {

enum class UnitCategory { Length, Mass, Temperature, DataSize };

// Display names for the categories and the units within a category, in a stable
// order suitable for populating UI pickers.
std::vector<std::string> CategoryNames();
std::vector<std::string> UnitsFor(UnitCategory category);

// Convert `value` from one unit to another within a category. nullopt if either
// unit name is not recognised for the category.
std::optional<double> ConvertUnit(UnitCategory category,
                                  const std::string& fromUnit,
                                  const std::string& toUnit,
                                  double value);

// --- Number bases (2/8/10/16) ---
// Parse `text` as an integer in `base` (2..36). nullopt if it isn't valid.
std::optional<long long> ParseInBase(const std::string& text, int base);
// Format `value` in `base` (uppercase for hex). Negative values get a '-' prefix.
std::string FormatInBase(long long value, int base);

}  // namespace superwin
