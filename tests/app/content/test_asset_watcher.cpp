#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

#include "content/asset_watcher.hpp"

namespace
{
    class TemporaryDirectory
    {
    public:
        TemporaryDirectory()
            : path(
                  std::filesystem::temp_directory_path() /
                  std::format(
                      "advanced_platformer_watch_{}",
                      std::chrono::steady_clock::now().time_since_epoch().count()))
        {
            std::filesystem::create_directories(path);
        }

        ~TemporaryDirectory()
        {
            std::filesystem::remove_all(path);
        }

        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

        std::filesystem::path path;
    };

    void write(const std::filesystem::path& file, const std::string& text)
    {
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file) << text;
    }
}

TEST_CASE("A watcher reports a change once the files stop changing", "[app][reload]")
{
    const TemporaryDirectory directory;
    write(directory.path / "levels" / "level_1.json", "{}");
    advanced_platformer::AssetWatcher watcher(directory.path);
    REQUIRE_FALSE(watcher.poll());

    write(directory.path / "levels" / "level_1.json", "{ }");

    REQUIRE_FALSE(watcher.poll());
    REQUIRE(watcher.poll());
    REQUIRE_FALSE(watcher.poll());
}

TEST_CASE("A watcher waits while files keep changing", "[app][reload]")
{
    const TemporaryDirectory directory;
    advanced_platformer::AssetWatcher watcher(directory.path);

    write(directory.path / "a.lua", "return {}");
    REQUIRE_FALSE(watcher.poll());
    write(directory.path / "b.lua", "return {}");
    REQUIRE_FALSE(watcher.poll());

    REQUIRE(watcher.poll());
}

TEST_CASE("A watcher sees new times, removed files and new files", "[app][reload]")
{
    const TemporaryDirectory directory;
    const std::filesystem::path file = directory.path / "tiles.json";
    write(file, "{}");
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
        write(directory.path / "scripts" / "rat.lua", "return {}");
    }

    REQUIRE_FALSE(watcher.poll());
    REQUIRE(watcher.poll());
}
