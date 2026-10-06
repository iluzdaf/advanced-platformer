#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <stdexcept>
#include <string>

#include <catch2/matchers/catch_matchers_string.hpp>

#include "content/level_generator.hpp"
#include "content/run_settings.hpp"
#include "support/json_document.hpp"

TEST_CASE("Run settings name the pieces and how rooms grow", "[app][content][json]")
{
    const auto run = advanced_platformer::parseRunSettings(
        R"({"pieces": "areas/rooms.json", "grid": [5, 3], "firstRooms": 4, "roomsPerLevel": 2, "maxRooms": 9})",
        "test run",
        "levels");

    REQUIRE(run.levelDirectory == std::filesystem::path("levels"));
    REQUIRE(run.relativePieces == std::filesystem::path("areas/rooms.json"));
    REQUIRE(run.grid.width == 5);
    REQUIRE(run.grid.height == 3);
    REQUIRE(run.firstRooms == 4);
    REQUIRE(run.roomsPerLevel == 2);
    REQUIRE(run.maxRooms == 9);
}

TEST_CASE("Each level adds rooms until the cap", "[app][content][json]")
{
    const auto run = advanced_platformer::parseRunSettings(
        R"({"pieces": "rooms.json", "firstRooms": 4, "roomsPerLevel": 2, "maxRooms": 9})",
        "test run");

    REQUIRE(advanced_platformer::roomsForLevel(run, 1) == 4);
    REQUIRE(advanced_platformer::roomsForLevel(run, 2) == 6);
    REQUIRE(advanced_platformer::roomsForLevel(run, 3) == 8);
    REQUIRE(advanced_platformer::roomsForLevel(run, 4) == 9);
    REQUIRE(advanced_platformer::roomsForLevel(run, 1000000) == 9);
    REQUIRE_THROWS_AS(advanced_platformer::roomsForLevel(run, 0), std::invalid_argument);
    const advanced_platformer::LevelGeneration generation =
        advanced_platformer::levelGeneration(run, 2, 17);
    REQUIRE(generation.roomCount == 6);
    REQUIRE(generation.seed == 17);
    REQUIRE(generation.grid.width == 9);
    REQUIRE(generation.grid.height == 7);
}

TEST_CASE("Level seeds follow from the run seed", "[app][content][json]")
{
    REQUIRE(advanced_platformer::runLevelSeed(5, 1) == advanced_platformer::runLevelSeed(5, 1));
    REQUIRE(advanced_platformer::runLevelSeed(5, 1) != advanced_platformer::runLevelSeed(5, 2));
    REQUIRE(advanced_platformer::runLevelSeed(5, 1) != advanced_platformer::runLevelSeed(6, 1));
    REQUIRE(advanced_platformer::nextRunSeed(5) == advanced_platformer::nextRunSeed(5));
    REQUIRE(advanced_platformer::nextRunSeed(5) != 5);
}

TEST_CASE("Run settings reject runs they cannot build", "[app][content][json]")
{
    auto runJson = tests::parseJson(
        R"({"pieces":"rooms.json","firstRooms":4,"roomsPerLevel":1,"maxRooms":6})");
    std::string message;
    SECTION("No pieces")
    {
        tests::eraseKey(runJson, "pieces");
        message = "run.json:";
    }
    SECTION("A piece file outside the level directory")
    {
        runJson["pieces"] = "../rooms.json";
        message = "pieces: expected a path inside the level directory";
    }
    SECTION("Too few first rooms")
    {
        runJson["firstRooms"] = 1;
        message = "firstRooms: expected at least 2 rooms and no more than maxRooms";
    }
    SECTION("More first rooms than the cap")
    {
        runJson["firstRooms"] = 7;
        message = "firstRooms: expected at least 2 rooms and no more than maxRooms";
    }
    SECTION("Fewer rooms each level")
    {
        runJson["roomsPerLevel"] = -1;
        message = "roomsPerLevel: expected zero or more rooms";
    }
    SECTION("A cap above what the grid holds")
    {
        runJson["grid"] = tests::numbers({2, 2});
        runJson["firstRooms"] = 2;
        message = "no more than the grid's 4 slots";
    }
    SECTION("An empty grid")
    {
        runJson["grid"] = tests::numbers({0, 3});
        message = "grid: expected a positive size";
    }
    SECTION("A number above int range")
    {
        runJson["maxRooms"] = 4294967297LL;
        message = "run.json:";
    }
    SECTION("A field typo")
    {
        runJson["piecess"] = "two.json";
        message = "run.json:";
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseRunSettings(tests::dumpJson(runJson), "run.json"),
        Catch::Matchers::ContainsSubstring(message));
}

TEST_CASE("Missing run settings are rejected at the file boundary", "[app][content][json]")
{
    REQUIRE_THROWS_AS(
        advanced_platformer::loadRunSettings("tests/fixtures/levels/does_not_exist.json"),
        std::invalid_argument);
}
