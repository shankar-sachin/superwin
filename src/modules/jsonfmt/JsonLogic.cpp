#include "modules/jsonfmt/JsonLogic.h"

#include <nlohmann/json.hpp>

namespace superwin {

JsonResult FormatJson(const std::string& input, int indent) {
    JsonResult r;
    try {
        // allow_exceptions=true (default); reject trailing junk; no comments.
        const nlohmann::json j = nlohmann::json::parse(input);
        r.text = (indent < 0) ? j.dump() : j.dump(indent);
        r.ok = true;
    } catch (const nlohmann::json::parse_error& e) {
        r.error = e.what();
    } catch (const std::exception& e) {
        r.error = e.what();
    }
    return r;
}

}  // namespace superwin
