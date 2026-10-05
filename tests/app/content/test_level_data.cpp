#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "content/level_data.hpp"
#include "advanced_platformer/math/coordinates.hpp"

TEST_CASE("Level JSON accepts a custom tile legend", "[app][content][json]")
{
    const auto data = advanced_platformer::parseLevelData(
        R"({
            "tileLegend": {".": "empty", "G": "grass", "X": "glass"},
            "map": [".GX"],
            "playerSpawn": {"cell": [0, 0]},
            "actors": [],
            "pickups": [],
            "exit": {"definition": "test_door", "spawn": {"cell": [2, 0]}}
        })",
        "custom level");

    REQUIRE(data.tileLegend.at('G') == "grass");
    REQUIRE(data.mapRows.front() == ".GX");
}

TEST_CASE(
    "Actor placements retain ids, definition references and patrol coordinates",
    "[app][content][json]")
{
    const auto data = advanced_platformer::parseLevelData(
        R"({
            "tileLegend": {".": "empty", "#": "stone"},
            "map": ["....", "####"],
            "playerSpawn": {"cell": [1, 0]},
            "actors": [
                {"id": "walker", "definition": "zombie", "spawn": {"cell": [2, 0]},
                 "patrol": {"first": {"cell": [2, 0]}, "second": {"cell": [3, 0]}}},
                {"id": "flyer", "definition": "bat", "spawn": {"feet": [17, 9]},
                 "patrol": {"first": {"feet": [17, 9]}, "second": {"feet": [25, 13]}}}
            ],
            "exit": {"definition": "test_door", "spawn": {"cell": [3, 0]}}
        })",
        "test level");

    REQUIRE(data.mapRows.size() == 2);
    REQUIRE(
        data.playerSpawn == advanced_platformer::LevelPosition{advanced_platformer::Cell{1, 0}});
    REQUIRE(data.actors.size() == 2);
    REQUIRE(data.actors[0].id == "walker");
    REQUIRE(data.actors[0].definitionName == "zombie");
    REQUIRE(
        data.actors[0].spawn ==
        advanced_platformer::LevelPosition{advanced_platformer::Cell{2, 0}});
    REQUIRE(data.actors[0].patrol.has_value());
    REQUIRE(
        data.actors[0].patrol.value_or(advanced_platformer::PatrolPlacement{}).second ==
        advanced_platformer::LevelPosition{advanced_platformer::Cell{3, 0}});
    REQUIRE(data.actors[1].id == "flyer");
    REQUIRE(data.actors[1].definitionName == "bat");
    REQUIRE(data.actors[1].spawn == advanced_platformer::LevelPosition{glm::vec2{17.0F, 9.0F}});
}

TEST_CASE("Explicit pickups and exits retain item requirements", "[app][content][json]")
{
    const auto data = advanced_platformer::parseLevelData(
        R"({
            "tileLegend": {".": "empty", "#": "stone"},
            "map": ["....", "####"],
            "playerSpawn": {"cell": [1, 0]},
            "pickups": [{"id": "key", "definition": "key", "spawn": {"cell": [1, 0]}}],
            "exit": {"definition": "test_door", "spawn": {"cell": [2, 0]},
                     "requirement": {"item": "key", "quantity": 1}}
        })",
        "test level");

    REQUIRE(data.pickups.size() == 1);
    REQUIRE(data.pickups[0].id == "key");
    REQUIRE(data.pickups[0].definitionName == "key");
    REQUIRE(
        data.pickups[0].spawn ==
        advanced_platformer::LevelPosition{advanced_platformer::Cell{1, 0}});
    REQUIRE(data.exit.spawn == advanced_platformer::LevelPosition{advanced_platformer::Cell{2, 0}});
    REQUIRE(data.exit.requirement.has_value());
    REQUIRE_FALSE(data.exit.nextLevel.has_value());
}

TEST_CASE("Pickup placements can reference a definition", "[app][content][json]")
{
    const auto data = advanced_platformer::parseLevelData(
        R"({
            "tileLegend": {".": "empty", "#": "stone"},
            "map": ["....", "####"],
            "playerSpawn": {"cell": [0, 0]},
            "actors": [],
            "pickups": [{"id": "chest", "definition": "treasure", "spawn": {"cell": [1, 0]}}],
            "exit": {"definition": "test_door", "spawn": {"cell": [3, 0]}}
        })",
        "placement.json");

    REQUIRE(data.pickups[0].definitionName == "treasure");
    REQUIRE(data.pickupReferences.at("pickups[0].definition") == "treasure");
}
