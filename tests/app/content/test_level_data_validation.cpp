#include <filesystem>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "content/level_data.hpp"
#include "support/json_document.hpp"

namespace
{
    tests::Json minimalLevel()
    {
        return tests::parseJson(R"({
            "tileLegend": {".": "empty", "#": "stone"},
            "map": ["....", "####"],
            "playerSpawnCell": [0, 0],
            "actors": [],
            "pickups": [],
            "exit": {"definition": "door", "spawnCell": [3, 0]}
        })");
    }
}

TEST_CASE(
    "Level integer fields reject values outside their destination types",
    "[app][content][json]")
{
    auto level = minimalLevel();
    SECTION("Oversized cell")
    {
        level["playerSpawnCell"][0] = 4294967296LL;
    }
    SECTION("Undersized cell")
    {
        level["playerSpawnCell"][1] = -4294967296LL;
    }
    SECTION("Exit destination")
    {
        level["exit"]["nextLevel"] = 4294967297LL;
    }
    REQUIRE_THROWS_AS(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "placements.json"),
        std::invalid_argument);
}

TEST_CASE("Explicit level placements reject unknown fields", "[app][content][json]")
{
    auto level = minimalLevel();
    SECTION("Root")
    {
        level["actorrs"] = tests::emptyArray();
    }
    SECTION("Actor")
    {
        level["actors"] = tests::list({tests::object(
            {{"id", "guard"},
             {"definition", "guard"},
             {"spawnCell", tests::numbers({1, 0})},
             {"patroll", tests::emptyObject()}})});
    }
    SECTION("Patrol")
    {
        level["actors"] = tests::list({tests::object(
            {{"id", "guard"},
             {"definition", "guard"},
             {"spawnCell", tests::numbers({1, 0})},
             {"patrol",
              tests::object(
                  {{"firstCell", tests::numbers({0, 0})},
                   {"secondCell", tests::numbers({1, 0})},
                   {"speeed", 1}})}})});
    }
    SECTION("Pickup")
    {
        level["pickups"] = tests::list({tests::object(
            {{"id", "key"},
             {"definition", "key"},
             {"spawnCell", tests::numbers({2, 0})},
             {"quantitty", 3}})});
    }
    SECTION("Exit")
    {
        level["exit"]["nextLevell"] = 2;
    }
    SECTION("Exit requirement")
    {
        level["exit"]["requirement"] =
            tests::object({{"item", "key"}, {"quantity", 1}, {"consumme", true}});
    }
    REQUIRE_THROWS_AS(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "placements.json"),
        std::invalid_argument);
}

TEST_CASE("Present placement lists must be arrays", "[app][content][json]")
{
    auto level = minimalLevel();
    level["actors"] = tests::emptyObject();
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "placements.json"),
        Catch::Matchers::StartsWith("placements.json: line 1, column ") &&
            Catch::Matchers::EndsWith("expected a list, found '{'"));

    level = minimalLevel();
    level["pickups"] = "coins";
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "placements.json"),
        Catch::Matchers::StartsWith("placements.json: line 1, column ") &&
            Catch::Matchers::EndsWith("expected a list, found 'coins'"));

    level = minimalLevel();
    level["pickups"] = nullptr;
    REQUIRE(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "placements.json")
            .pickups.empty());
}

TEST_CASE("Levels may omit placement arrays", "[app][content][json]")
{
    const auto data = advanced_platformer::parseLevelData(
        R"({
            "tileLegend": {".": "empty"},
            "map": ["..."],
            "playerSpawnCell": [0, 0],
            "exit": {"definition": "test_door", "spawnCell": [2, 0]}
        })",
        "empty level");

    REQUIRE(data.actors.empty());
    REQUIRE(data.pickups.empty());
}

TEST_CASE("Tile legend keys must be one character", "[app][content][json]")
{
    auto level = minimalLevel();
    level["tileLegend"]["long"] = "grass";

    REQUIRE_THROWS_AS(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "bad legend"),
        std::invalid_argument);
}

TEST_CASE("Map symbols must be declared in a legend", "[app][content][json]")
{
    auto level = minimalLevel();
    level["map"][0] = ".X..";

    REQUIRE_THROWS_AS(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "bad symbol"),
        std::invalid_argument);
}

TEST_CASE("Level maps require rectangular rows", "[app][content][json]")
{
    auto level = minimalLevel();
    level["map"][1] = "###";

    REQUIRE_THROWS_AS(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "ragged level"),
        std::invalid_argument);
}

TEST_CASE("Actor placements need a non-empty definition", "[app][content][json]")
{
    auto level = minimalLevel();
    level["actors"] = tests::list({tests::object(
        {{"id", "guard"}, {"definition", ""}, {"spawnCell", tests::numbers({1, 0})}})});

    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "level.json"),
        "level.json: actors[0].definition: actor definition name cannot be empty");
}

TEST_CASE("Actor placements choose a cell or feet, not both", "[app][content][json]")
{
    auto level = minimalLevel();
    level["actors"] = tests::list({tests::object(
        {{"id", "guard"},
         {"definition", "guard"},
         {"spawnCell", tests::numbers({1, 0})},
         {"spawnFeet", tests::numbers({24, 16})}})});

    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "level.json"),
        "level.json: actors[0]: supply exactly one of 'spawnCell' or 'spawnFeet'");
}

TEST_CASE("Pickup placements take their stack from a definition", "[app][content][json]")
{
    auto level = minimalLevel();
    level["pickups"] = tests::list({tests::object(
        {{"id", "chest"}, {"definition", "treasure"}, {"spawnCell", tests::numbers({1, 0})}})});
    const char* expected = "";
    SECTION("No definition")
    {
        tests::eraseKey(level["pickups"][0], "definition");
        expected = "missing 'definition'";
    }
    SECTION("An inline item")
    {
        level["pickups"][0]["item"] = "key";
        expected = "unknown field 'item'";
    }
    SECTION("An inline quantity")
    {
        level["pickups"][0]["quantity"] = 2;
        expected = "unknown field 'quantity'";
    }
    SECTION("An inline body size")
    {
        level["pickups"][0]["bodySize"] = tests::numbers({8, 8});
        expected = "unknown field 'bodySize'";
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "placement.json"),
        Catch::Matchers::ContainsSubstring(expected));
}

TEST_CASE("Pickup definition names cannot be empty", "[app][content][json]")
{
    auto level = minimalLevel();
    level["pickups"] = tests::list({tests::object(
        {{"id", "chest"}, {"definition", ""}, {"spawnCell", tests::numbers({1, 0})}})});

    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "level.json"),
        "level.json: pickups[0].definition: pickup definition name cannot be empty");
}

TEST_CASE("Missing level JSON is rejected at the file boundary", "[app][content][json]")
{
    REQUIRE_THROWS_AS(
        advanced_platformer::loadLevelData(
            std::filesystem::path("assets/levels/does_not_exist.json")),
        std::invalid_argument);
}

TEST_CASE("Level JSON requires a tile legend", "[app][content][json]")
{
    auto level = minimalLevel();
    tests::eraseKey(level, "tileLegend");

    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "no legend"),
        Catch::Matchers::ContainsSubstring("tileLegend"));
}
