// In-memory clipboard history with JSON persistence.
//
// This is pure data/logic: capturing OS clipboard events lives in ClipWatcher,
// and rendering lives in ClipPage / ClipPicker. Keeping the store independent
// makes the dedupe / pin / eviction rules straightforward to unit-test.
#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace superwin {

struct ClipItem {
    enum class Kind { Text, Image };

    uint64_t    id = 0;            // stable handle for the UI
    Kind        kind = Kind::Text;
    std::string text;             // UTF-8 text, or image cache filename for images
    bool        pinned = false;
    int64_t     timestamp = 0;     // unix seconds
};

class ClipStore {
public:
    // `maxItems` bounds the unpinned history; pinned items are always kept.
    // `path` defaults to %APPDATA%\SuperWin\clipboard.json.
    explicit ClipStore(int maxItems = 50, std::filesystem::path path = {});

    // Add a text clip. Identical content is de-duplicated and moved to front.
    // Returns the id of the (new or promoted) item.
    uint64_t AddText(std::string utf8);

    bool Pin(uint64_t id, bool pinned);
    bool Remove(uint64_t id);
    void Clear(bool includePinned);

    // Newest-first snapshot, optionally filtered by a case-insensitive substring.
    std::vector<ClipItem> Items(const std::string& filter = {}) const;

    std::optional<ClipItem> Get(uint64_t id) const;
    size_t Size() const { return items_.size(); }

    void SetMaxItems(int maxItems);
    int  MaxItems() const { return maxItems_; }

    void Load();
    void Save() const;

private:
    void EvictToCapacity();

    std::deque<ClipItem>  items_;  // front == most recent
    int                   maxItems_;
    uint64_t              nextId_ = 1;
    std::filesystem::path path_;
};

}  // namespace superwin
