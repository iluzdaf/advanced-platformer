#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "content/level_data.hpp"
#include "support/json_document.hpp"

namespace
{
    tests::Json markerLevel()
    {
        return tests::parseJson(R"({
            "tileLegend": {".": "empty", "#": "stone"},
            "objectLegend": {
                "P": {"type": "player"},
                "E": {"type": "exit", "definition": "test_door"}
            },
            "map": ["PE"]
        })");
    }
}

TEST_CASE("Object markers require exactly one player and exit", "[app][content][json]")
{
    auto level = markerLevel();
    SECTION("Repeated player")
    {
        level["map"] = tests::list({"PPE"});
    }
    SECTION("Repeated exit")
    {
        level["map"] = tests::list({"PEE"});
    }
    SECTION("Missing player")
    {
        level["map"] = tests::list({".E"});
    }
    SECTION("Missing exit")
    {
        level["map"] = tests::list({"P."});
    }
    REQUIRE_THROWS_AS(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "bad markers"),
        std::invalid_argument);
}

TEST_CASE("Object markers cannot duplicate explicit placements", "[app][content][json]")
{
    auto level = markerLevel();
    SECTION("Player")
    {
        level["playerSpawnCell"] = tests::numbers({0, 0});
    }
    SECTION("Exit")
    {
        level["exit"] =
            tests::object({{"definition", "test_door"}, {"spawnCell", tests::numbers({1, 0})}});
    }
    REQUIRE_THROWS_AS(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "bad markers"),
        std::invalid_argument);
}

TEST_CASE("Object legend symbols are single glyphs distinct from terrain", "[app][content][json]")
{
    auto level = markerLevel();
    SECTION("Terrain symbol")
    {
        level["objectLegend"]["#"] = tests::object({{"type", "actor"}, {"definition", "zombie"}});
    }
    SECTION("Long symbol")
    {
        level["objectLegend"]["ZZ"] = tests::object({{"type", "actor"}, {"definition", "zombie"}});
    }
    REQUIRE_THROWS_AS(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "bad markers"),
        std::invalid_argument);
}

TEST_CASE(
    "Unused object templates still need a valid category and definition",
    "[app][content][json]")
{
    auto level = markerLevel();
    SECTION("Empty actor definition")
    {
        level["objectLegend"]["Z"] = tests::object({{"type", "actor"}, {"definition", ""}});
    }
    SECTION("Missing actor definition")
    {
        level["objectLegend"]["Z"] = tests::object({{"type", "actor"}});
    }
    SECTION("Unknown category")
    {
        level["objectLegend"]["Z"] = tests::object({{"type", "zombie"}, {"definition", "zombie"}});
    }
    REQUIRE_THROWS_AS(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "bad markers"),
        std::invalid_argument);
}

TEST_CASE("Object templates cannot provide a placement", "[app][content][json]")
{
    auto level = markerLevel();
    level["objectLegend"]["P"]["spawnFeet"] = tests::numbers({1, 2});

    REQUIRE_THROWS_AS(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "bad markers"),
        std::invalid_argument);
}

TEST_CASE("Pickup object templates name a definition", "[app][content][json]")
{
    auto level = markerLevel();
    const char* expected = "";
    SECTION("No definition")
    {
        level["objectLegend"]["K"] = tests::object({{"type", "pickup"}});
        expected = "missing 'definition'";
    }
    SECTION("An inline stack")
    {
        level["objectLegend"]["K"] = tests::object(
            {{"type", "pickup"},
             {"item", "key"},
             {"quantity", 1},
             {"bodySize", tests::numbers({8, 8})}});
        expected = "unknown field 'item'";
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "bad markers"),
        Catch::Matchers::ContainsSubstring(expected));
}

TEST_CASE("Unused object templates reject unknown fields", "[app][content][json]")
{
    auto level = markerLevel();
    SECTION("Actor")
    {
        level["objectLegend"]["Z"] =
            tests::object({{"type", "actor"}, {"definition", "guard"}, {"patroll", true}});
    }
    SECTION("Player")
    {
        level["objectLegend"]["Q"] = tests::object({{"type", "player"}, {"health", 4}});
    }
    SECTION("Pickup")
    {
        level["objectLegend"]["K"] =
            tests::object({{"type", "pickup"}, {"definition", "key"}, {"quantitty", 3}});
    }
    SECTION("Exit")
    {
        level["objectLegend"]["X"] =
            tests::object({{"type", "exit"}, {"definition", "door"}, {"nextLevell", 2}});
    }
    REQUIRE_THROWS_AS(
        advanced_platformer::parseLevelData(tests::dumpJson(level), "bad markers"),
        std::invalid_argument);
}
