// JSON formatter/validator. Pure logic in superwin_core (unit-tested), built on
// the vendored nlohmann/json. Pretty-print, minify, and validate arbitrary JSON.
#pragma once

#include <string>

namespace superwin {

struct JsonResult {
    bool        ok = false;
    std::string text;   // formatted output when ok
    std::string error;  // human-readable parse error when !ok
};

// Parse `input` and re-emit it. `indent < 0` minifies (no whitespace); otherwise
// pretty-prints with that many spaces per level. On a parse error, `ok` is false
// and `error` describes the problem.
JsonResult FormatJson(const std::string& input, int indent);

}  // namespace superwin
