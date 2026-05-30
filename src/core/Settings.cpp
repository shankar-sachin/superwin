#include "core/Settings.h"

#include <Windows.h>
#include <ShlObj.h>  // SHGetKnownFolderPath

#include <fstream>
#include <sstream>
#include <vector>

#include "core/Strings.h"

namespace superwin {
namespace {

// Split a dotted path "a.b.c" into its components.
std::vector<std::string> SplitPath(std::string_view path) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= path.size()) {
        const size_t dot = path.find('.', start);
        if (dot == std::string_view::npos) {
            parts.emplace_back(path.substr(start));
            break;
        }
        parts.emplace_back(path.substr(start, dot - start));
        start = dot + 1;
    }
    return parts;
}

}  // namespace

Settings& Settings::Instance() {
    static Settings instance;
    return instance;
}

std::filesystem::path Settings::DataDirectory() {
    PWSTR raw = nullptr;
    std::filesystem::path dir;
    if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &raw))) {
        dir = std::filesystem::path(raw) / L"SuperWin";
    }
    if (raw) ::CoTaskMemFree(raw);
    return dir;
}

std::filesystem::path Settings::SettingsPath() {
    return DataDirectory() / L"settings.json";
}

Settings::Settings() : path_(SettingsPath()) {
    Load();
}

void Settings::Load() {
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);

    std::ifstream in(path_, std::ios::binary);
    if (in) {
        try {
            in >> root_;
        } catch (const std::exception&) {
            root_ = nlohmann::json::object();  // corrupt file -> start fresh
        }
    }
    if (!root_.is_object()) root_ = nlohmann::json::object();
}

void Settings::Flush() const {
    // Atomic-ish write: serialize to a temp file then move over the target.
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    const std::filesystem::path tmp = path_.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return;
        out << root_.dump(2);
    }
    std::filesystem::rename(tmp, path_, ec);
    if (ec) {  // rename can fail if target is locked; fall back to copy
        std::filesystem::copy_file(tmp, path_,
            std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(tmp, ec);
    }
}

const nlohmann::json* Settings::Find(std::string_view path) const {
    const nlohmann::json* node = &root_;
    for (const auto& key : SplitPath(path)) {
        if (!node->is_object()) return nullptr;
        const auto it = node->find(key);
        if (it == node->end()) return nullptr;
        node = &*it;
    }
    return node;
}

template <typename T>
void Settings::Assign(std::string_view path, T&& value) {
    std::lock_guard lock(mutex_);
    nlohmann::json* node = &root_;
    for (const auto& key : SplitPath(path)) {
        if (!node->is_object()) *node = nlohmann::json::object();
        node = &(*node)[key];
    }
    *node = std::forward<T>(value);
    Flush();
}

bool Settings::GetBool(std::string_view path, bool fallback) const {
    std::lock_guard lock(mutex_);
    const auto* n = Find(path);
    return (n && n->is_boolean()) ? n->get<bool>() : fallback;
}

int Settings::GetInt(std::string_view path, int fallback) const {
    std::lock_guard lock(mutex_);
    const auto* n = Find(path);
    return (n && n->is_number_integer()) ? n->get<int>() : fallback;
}

double Settings::GetDouble(std::string_view path, double fallback) const {
    std::lock_guard lock(mutex_);
    const auto* n = Find(path);
    return (n && n->is_number()) ? n->get<double>() : fallback;
}

std::string Settings::GetString(std::string_view path, std::string_view fallback) const {
    std::lock_guard lock(mutex_);
    const auto* n = Find(path);
    return (n && n->is_string()) ? n->get<std::string>() : std::string(fallback);
}

void Settings::Set(std::string_view path, bool value)            { Assign(path, value); }
void Settings::Set(std::string_view path, int value)             { Assign(path, value); }
void Settings::Set(std::string_view path, double value)          { Assign(path, value); }
void Settings::Set(std::string_view path, std::string_view value){ Assign(path, std::string(value)); }

}  // namespace superwin
