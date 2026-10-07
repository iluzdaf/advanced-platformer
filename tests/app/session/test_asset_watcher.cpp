#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>

#include "session/asset_watcher.hpp"
#include "support/temporary_directory.hpp"

TEST_CASE("A watcher reports a change once the files stop changing", "[app][reload]")
{
    const tests::TemporaryDirectory directory;
    tests::writeFile(directory.path / "levels" / "level_1.json", "{}");
    advanced_platformer::AssetWatcher watcher(directory.path);
    REQUIRE_FALSE(watcher.poll());

    tests::writeFile(directory.path / "levels" / "level_1.json", "{ }");

    REQUIRE_FALSE(watcher.poll());
    REQUIRE(watcher.poll());
    REQUIRE_FALSE(watcher.poll());
}

TEST_CASE("A watcher waits while files keep changing", "[app][reload]")
{
    const tests::TemporaryDirectory directory;
    advanced_platformer::AssetWatcher watcher(directory.path);

    tests::writeFile(directory.path / "a.lua", "return {}");
    REQUIRE_FALSE(watcher.poll());
    tests::writeFile(directory.path / "b.lua", "return {}");
    REQUIRE_FALSE(watcher.poll());

    REQUIRE(watcher.poll());
}

TEST_CASE("A watcher sees new times, removed files and new files", "[app][reload]")
{
    const tests::TemporaryDirectory directory;
    const std::filesystem::path file = directory.path / "tiles.json";
    tests::writeFile(file, "{}");
    advanced_platformer::AssetWatcher watcher(directory.path);

    SECTION("A new modification time at the same size")
    {
        std::filesystem::last_write_time(
            file, std::filesystem::last_write_time(file) + std::chrono::seconds(1));
    }
    SECTION("A removed file")
    {
        std::filesystem::remove(file);
    }
    SECTION("A new file in a new directory")
    {
        tests::writeFile(directory.path / "scripts" / "rat.lua", "return {}");
    }

    REQUIRE_FALSE(watcher.poll());
    REQUIRE(watcher.poll());
}
