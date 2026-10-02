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
            "exit": {"definition": "test_door", "spawnCell": [3, 0]}
        })");
    }

    tests::Json placement(const char* id, const char* definition, double column)
    {
        return tests::object(
            {{"id", id}, {"definition", definition}, {"spawnCell", tests::numbers({column, 0})}});
    }
}

TEST_CASE("Levels place one player and one exit", "[app][content][json]")
{
    auto level = minimalLevel();
    const char* expected = "";
    SECTION("No player")
    {
        tests::eraseKey(level, "playerSpawnCell");
        expected = "level.json: root: supply exactly one of 'playerSpawnCell' or 'playerSpawnFeet'";
    }
    SECTION("A player by cell and by feet")
    {
        level["playerSpawnFeet"] = tests::numbers({4, 8});
        expected = "level.json: root: supply exactly one of 'playerSpawnCell' or 'playerSpawnFeet'";
    }
    SECTION("No exit")
    {
        tests::eraseKey(level, "exit");
        expected = "level.json: exit: expected exactly one placement";
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "level.json"), expected);
}

TEST_CASE("Levels reject an object legend", "[app][content][json]")
{
    auto level = minimalLevel();
    level["objectLegend"] = tests::object({{"Z", tests::object({{"type", "actor"}})}});

    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "level.json"),
        Catch::Matchers::ContainsSubstring("unknown field 'objectLegend'"));
}

TEST_CASE("Actor and pickup placements need an id", "[app][content][json]")
{
    auto level = minimalLevel();
    SECTION("Actor without an id")
    {
        level["actors"] = tests::list({placement("guard", "guard", 1)});
        tests::eraseKey(level["actors"][0], "id");
    }
    SECTION("Pickup without an id")
    {
        level["pickups"] = tests::list({placement("key", "key", 1)});
        tests::eraseKey(level["pickups"][0], "id");
    }
    REQUIRE_THROWS_AS(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "level.json"),
        std::invalid_argument);
}

TEST_CASE("Placement ids cannot be empty", "[app][content][json]")
{
    auto level = minimalLevel();
    const char* expected = "";
    SECTION("Actor")
    {
        level["actors"] = tests::list({placement("", "guard", 1)});
        expected = "level.json: actors[0].id: placement id cannot be empty";
    }
    SECTION("Pickup")
    {
        level["pickups"] = tests::list({placement("", "key", 1)});
        expected = "level.json: pickups[0].id: placement id cannot be empty";
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "level.json"), expected);
}

TEST_CASE("Placement ids are unique across a level's actors and pickups", "[app][content][json]")
{
    auto level = minimalLevel();
    const char* expected = "";
    SECTION("Two actors")
    {
        level["actors"] =
            tests::list({placement("guard", "guard", 1), placement("guard", "bat", 2)});
        expected = "level.json: actors[1].id: id 'guard' is already used by actors[0]";
    }
    SECTION("An actor and a pickup")
    {
        level["actors"] = tests::list({placement("guard", "guard", 1)});
        level["pickups"] = tests::list({placement("guard", "key", 2)});
        expected = "level.json: pickups[0].id: id 'guard' is already used by actors[0]";
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "level.json"), expected);
}

TEST_CASE("Distinct placement ids are accepted", "[app][content][json]")
{
    auto level = minimalLevel();
    level["actors"] = tests::list({placement("guard", "guard", 1), placement("bat", "bat", 2)});
    level["pickups"] = tests::list({placement("key", "key", 3)});

    const auto data = advanced_platformer::parseLevelData(tests::dumpJson(level), "level.json");

    REQUIRE(data.actors[0].id == "guard");
    REQUIRE(data.actors[1].id == "bat");
    REQUIRE(data.pickups[0].id == "key");
}
