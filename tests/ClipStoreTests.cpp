#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "modules/clipboard/ClipStore.h"

using namespace superwin;

namespace {
std::filesystem::path TempStorePath() {
    return std::filesystem::temp_directory_path() / "superwin_clipstore_test.json";
}
}  // namespace

TEST_CASE("adds text newest-first", "[clipboard]") {
    ClipStore store(10, TempStorePath());
    store.Clear(true);
    store.AddText("one");
    store.AddText("two");
    const auto items = store.Items();
    REQUIRE(items.size() == 2);
    REQUIRE(items[0].text == "two");
    REQUIRE(items[1].text == "one");
}

TEST_CASE("duplicate text is promoted, not duplicated", "[clipboard]") {
    ClipStore store(10, TempStorePath());
    store.Clear(true);
    store.AddText("a");
    store.AddText("b");
    store.AddText("a");  // promote existing "a" to front
    const auto items = store.Items();
    REQUIRE(items.size() == 2);
    REQUIRE(items[0].text == "a");
    REQUIRE(items[1].text == "b");
}

TEST_CASE("eviction respects the cap but keeps pinned", "[clipboard]") {
    ClipStore store(2, TempStorePath());
    store.Clear(true);
    const uint64_t first = store.AddText("first");
    store.Pin(first, true);
    store.AddText("second");
    store.AddText("third");
    store.AddText("fourth");  // pushes unpinned count past cap of 2

    const auto items = store.Items();
    // pinned "first" survives; only two most-recent unpinned remain
    bool hasFirst = false;
    int unpinned = 0;
    for (const auto& c : items) {
        if (c.text == "first") hasFirst = true;
        if (!c.pinned) ++unpinned;
    }
    REQUIRE(hasFirst);
    REQUIRE(unpinned == 2);
}

TEST_CASE("filter is case-insensitive substring", "[clipboard]") {
    ClipStore store(10, TempStorePath());
    store.Clear(true);
    store.AddText("Hello World");
    store.AddText("goodbye");
    REQUIRE(store.Items("WORLD").size() == 1);
    REQUIRE(store.Items("o").size() == 2);
    REQUIRE(store.Items("zzz").empty());
}

TEST_CASE("persists and reloads", "[clipboard]") {
    const auto path = TempStorePath();
    {
        ClipStore store(10, path);
        store.Clear(true);
        store.AddText("persist me");
        store.Save();
    }
    ClipStore reloaded(10, path);
    reloaded.Load();
    const auto items = reloaded.Items();
    REQUIRE(items.size() == 1);
    REQUIRE(items[0].text == "persist me");
}
