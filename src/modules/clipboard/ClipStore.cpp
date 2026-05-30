#include "modules/clipboard/ClipStore.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>

#include <nlohmann/json.hpp>

#include "core/Settings.h"

namespace superwin {
namespace {

int64_t NowSeconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

bool ContainsCI(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    const auto it = std::search(
        haystack.begin(), haystack.end(), needle.begin(), needle.end(),
        [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); });
    return it != haystack.end();
}

}  // namespace

ClipStore::ClipStore(int maxItems, std::filesystem::path path)
    : maxItems_(std::max(1, maxItems)),
      path_(path.empty() ? (Settings::DataDirectory() / L"clipboard.json") : std::move(path)) {}

uint64_t ClipStore::AddText(std::string utf8) {
    if (utf8.empty()) return 0;

    // Dedupe: if the same text already exists, promote it to the front.
    const auto existing = std::find_if(items_.begin(), items_.end(),
        [&](const ClipItem& c) { return c.kind == ClipItem::Kind::Text && c.text == utf8; });
    if (existing != items_.end()) {
        ClipItem promoted = *existing;
        promoted.timestamp = NowSeconds();
        items_.erase(existing);
        items_.push_front(promoted);
        Save();
        return promoted.id;
    }

    ClipItem item;
    item.id = nextId_++;
    item.kind = ClipItem::Kind::Text;
    item.text = std::move(utf8);
    item.timestamp = NowSeconds();
    items_.push_front(item);
    EvictToCapacity();
    Save();
    return item.id;
}

bool ClipStore::Pin(uint64_t id, bool pinned) {
    for (auto& c : items_) {
        if (c.id == id) {
            c.pinned = pinned;
            Save();
            return true;
        }
    }
    return false;
}

bool ClipStore::Remove(uint64_t id) {
    const auto it = std::find_if(items_.begin(), items_.end(),
        [&](const ClipItem& c) { return c.id == id; });
    if (it == items_.end()) return false;
    items_.erase(it);
    Save();
    return true;
}

void ClipStore::Clear(bool includePinned) {
    if (includePinned) {
        items_.clear();
    } else {
        items_.erase(std::remove_if(items_.begin(), items_.end(),
            [](const ClipItem& c) { return !c.pinned; }), items_.end());
    }
    Save();
}

std::vector<ClipItem> ClipStore::Items(const std::string& filter) const {
    std::vector<ClipItem> out;
    out.reserve(items_.size());
    for (const auto& c : items_) {
        if (ContainsCI(c.text, filter)) out.push_back(c);
    }
    return out;
}

std::optional<ClipItem> ClipStore::Get(uint64_t id) const {
    for (const auto& c : items_)
        if (c.id == id) return c;
    return std::nullopt;
}

void ClipStore::SetMaxItems(int maxItems) {
    maxItems_ = std::max(1, maxItems);
    EvictToCapacity();
    Save();
}

void ClipStore::EvictToCapacity() {
    // Count only unpinned items against the cap; evict oldest unpinned first.
    int unpinned = 0;
    for (const auto& c : items_) if (!c.pinned) ++unpinned;
    for (auto it = items_.rbegin(); it != items_.rend() && unpinned > maxItems_; ) {
        if (!it->pinned) {
            // erase via base iterator; recompute reverse iterator after erase
            it = std::make_reverse_iterator(items_.erase(std::next(it).base()));
            --unpinned;
        } else {
            ++it;
        }
    }
}

void ClipStore::Load() {
    std::ifstream in(path_, std::ios::binary);
    if (!in) return;
    nlohmann::json j;
    try { in >> j; } catch (const std::exception&) { return; }
    if (!j.is_array()) return;

    items_.clear();
    uint64_t maxId = 0;
    for (const auto& e : j) {
        ClipItem item;
        item.id = e.value("id", 0ull);
        item.kind = e.value("kind", 0) == 1 ? ClipItem::Kind::Image : ClipItem::Kind::Text;
        item.text = e.value("text", std::string{});
        item.pinned = e.value("pinned", false);
        item.timestamp = e.value("timestamp", int64_t{0});
        maxId = std::max(maxId, item.id);
        items_.push_back(std::move(item));
    }
    nextId_ = maxId + 1;
}

void ClipStore::Save() const {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& c : items_) {
        j.push_back({
            {"id", c.id},
            {"kind", c.kind == ClipItem::Kind::Image ? 1 : 0},
            {"text", c.text},
            {"pinned", c.pinned},
            {"timestamp", c.timestamp},
        });
    }
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    std::ofstream out(path_, std::ios::binary | std::ios::trunc);
    if (out) out << j.dump(2);
}

}  // namespace superwin
