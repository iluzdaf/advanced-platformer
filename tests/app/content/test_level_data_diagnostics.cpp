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
            "map": ["...", "###"],
            "playerSpawn": {"cell": [0, 0]},
            "exit": {"definition": "test_door", "spawn": {"cell": [2, 0]}}
        })");
    }
}

TEST_CASE("Map diagnostics identify authored cells and row widths", "[app][content][json]")
{
    auto level = minimalLevel();
    SECTION("Unknown symbol")
    {
        level["map"][0] = ".?.";
        REQUIRE_THROWS_WITH(
            advanced_platformer::parseLevelData(tests::dumpJson(level), "level.json"),
            "level.json: map[0][1]: unknown symbol '?'; define it in tileLegend");
    }
    SECTION("Row width")
    {
        level["map"][1] = "#";
        REQUIRE_THROWS_WITH(
            advanced_platformer::parseLevelData(tests::dumpJson(level), "level.json"),
            "level.json: map[1]: expected 3 columns, got 1");
    }
}

TEST_CASE("Placement diagnostics retain their paths", "[app][content][json]")
{
    auto level = minimalLevel();
    SECTION("Empty actor definition")
    {
        level["actors"] = tests::list({tests::object(
            {{"id", "guard"},
             {"definition", ""},
             {"spawn", tests::object({{"cell", tests::numbers({0, 0})}})}})});
        REQUIRE_THROWS_WITH(
            advanced_platformer::parseLevelData(tests::dumpJson(level), "level.json"),
            "level.json: actors[0].definition: actor definition name cannot be empty");
    }
    SECTION("Empty exit definition")
    {
        level["exit"]["definition"] = "";
        REQUIRE_THROWS_WITH(
            advanced_platformer::parseLevelData(tests::dumpJson(level), "level.json"),
            "level.json: exit.definition: exit definition name cannot be empty");
    }
    SECTION("Invalid exit setting")
    {
        level["exit"]["consumeItem"] = 1;
        REQUIRE_THROWS_WITH(
            advanced_platformer::parseLevelData(tests::dumpJson(level), "level.json"),
            Catch::Matchers::StartsWith("level.json: line 1, column ") &&
                Catch::Matchers::EndsWith("expected true or false"));
    }
}

TEST_CASE("JSON syntax diagnostics include one-based line and byte column", "[app][content][json]")
{
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelData("{\n  ?\n}", "broken.json"),
        Catch::Matchers::ContainsSubstring("broken.json: line 2, column 3: invalid JSON:"));
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelData("{\n", "unfinished.json"),
        Catch::Matchers::ContainsSubstring("unfinished.json: line 2, column 1: invalid JSON:"));
}
