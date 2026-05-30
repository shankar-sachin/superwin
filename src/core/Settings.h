// Application settings persisted as JSON in %APPDATA%\SuperWin\settings.json.
//
// Thin typed wrapper over nlohmann::json. Values are addressed with simple
// dotted paths (e.g. "clipboard.maxItems"). The store is loaded once at
// startup and written back atomically whenever a value changes (or on Flush).
#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace superwin {

class Settings {
public:
    // Returns the process-wide instance, loading from disk on first use.
    static Settings& Instance();

    // Typed accessors. The dotted `path` is created on demand for setters.
    bool        GetBool(std::string_view path, bool fallback) const;
    int         GetInt(std::string_view path, int fallback) const;
    double      GetDouble(std::string_view path, double fallback) const;
    std::string GetString(std::string_view path, std::string_view fallback) const;

    void Set(std::string_view path, bool value);
    void Set(std::string_view path, int value);
    void Set(std::string_view path, double value);
    void Set(std::string_view path, std::string_view value);

    // Force a write to disk. Setters already persist, but call this after a
    // batch of edits if you suppressed auto-save.
    void Flush() const;

    // Location of the settings file (also the per-user data directory parent).
    static std::filesystem::path DataDirectory();
    static std::filesystem::path SettingsPath();

private:
    Settings();
    void Load();

    const nlohmann::json* Find(std::string_view path) const;
    template <typename T>
    void Assign(std::string_view path, T&& value);

    mutable std::mutex     mutex_;
    nlohmann::json         root_;
    std::filesystem::path  path_;
};

}  // namespace superwin
