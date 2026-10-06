#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <stdexcept>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <glm/vec2.hpp>
#include "content/level_catalog.hpp"
#include "support/json_document.hpp"

TEST_CASE("Level catalog numbers reject narrowing and fields reject typos", "[app][content][json]")
{
    auto levelCatalogJson = tests::parseJson(
        R"({"startLevel":1,"cameraDeadZone":[80,45],"levels":[{"number":1,"file":"one.json"}]})");
    SECTION("Above int range")
    {
        levelCatalogJson["startLevel"] = 4294967297LL;
    }
    SECTION("Below int range")
    {
        levelCatalogJson["levels"][0]["number"] = -4294967295LL;
    }
    SECTION("Top-level field typo")
    {
        levelCatalogJson["startLevell"] = 1;
    }
    SECTION("Entry typo")
    {
        levelCatalogJson["levels"][0]["fille"] = "two.json";
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelCatalog(tests::dumpJson(levelCatalogJson), "levels.json"),
        Catch::Matchers::ContainsSubstring("levels.json:"));
}

TEST_CASE("A level catalog maps stable IDs to arbitrary file names", "[app][content][json]")
{
    const auto catalog = advanced_platformer::parseLevelCatalog(
        R"({
            "startLevel": 10,
            "cameraDeadZone": [80, 45],
            "levels": [
                {"number": 10, "file": "opening.json"},
                {"number": 25, "file": "areas/final_room.json"}
            ]
        })",
        "test catalog",
        "levels");

    REQUIRE(catalog.startLevel == 10);
    REQUIRE(catalog.cameraDeadZone == glm::vec2{80.0F, 45.0F});
    REQUIRE(catalog.levels.size() == 2);
    REQUIRE(
        advanced_platformer::levelPath(catalog, 25) ==
        std::filesystem::path("levels/areas/final_room.json"));
    REQUIRE_THROWS_AS(advanced_platformer::levelPath(catalog, 1), std::invalid_argument);
}

TEST_CASE("A level catalog rejects ambiguous or unsafe entries", "[app][content][json]")
{
    SECTION("start level is not listed")
    {
        REQUIRE_THROWS_AS(
            advanced_platformer::parseLevelCatalog(
                R"({"startLevel": 2, "cameraDeadZone": [80, 45], "levels": [{"number": 1, "file": "one.json"}]})",
                "test catalog"),
            std::invalid_argument);
    }

    SECTION("level ID is duplicated")
    {
        REQUIRE_THROWS_AS(
            advanced_platformer::parseLevelCatalog(
                R"({
                    "startLevel": 1,
                    "cameraDeadZone": [80, 45],
                    "levels": [
                        {"number": 1, "file": "one.json"},
                        {"number": 1, "file": "another.json"}
                    ]
                })",
                "test catalog"),
            std::invalid_argument);
    }

    SECTION("file escapes the level directory")
    {
        REQUIRE_THROWS_AS(
            advanced_platformer::parseLevelCatalog(
                R"({"startLevel": 1, "cameraDeadZone": [80, 45], "levels": [{"number": 1, "file": "../one.json"}]})",
                "test catalog"),
            std::invalid_argument);
    }
}

TEST_CASE("A missing level catalog is rejected at the file boundary", "[app][content][json]")
{
    REQUIRE_THROWS_AS(
        advanced_platformer::loadLevelCatalog("tests/fixtures/levels/does_not_exist.json"),
        std::invalid_argument);
}

TEST_CASE(
    "A level catalog's camera dead zone is positive and fits in the view",
    "[app][content][json]")
{
    auto levelCatalogJson = tests::parseJson(
        R"({"startLevel":1,"cameraDeadZone":[80,45],"levels":[{"number":1,"file":"one.json"}]})");
    SECTION("Missing")
    {
        tests::eraseKey(levelCatalogJson, "cameraDeadZone");
    }
    SECTION("Empty")
    {
        levelCatalogJson["cameraDeadZone"] = tests::numbers({0, 45});
    }
    SECTION("Wider than the view")
    {
        levelCatalogJson["cameraDeadZone"] = tests::numbers({321, 45});
    }
    SECTION("Taller than the view")
    {
        levelCatalogJson["cameraDeadZone"] = tests::numbers({80, 181});
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelCatalog(tests::dumpJson(levelCatalogJson), "levels.json"),
        Catch::Matchers::ContainsSubstring("cameraDeadZone"));
}

TEST_CASE("A level catalog entry can generate its level from room pieces", "[app][content][json]")
{
    const auto catalog = advanced_platformer::parseLevelCatalog(
        R"({
            "startLevel": 1,
            "cameraDeadZone": [80, 45],
            "levels": [
                {"number": 1, "generate": {"pieces": "rooms.json", "rooms": 6, "grid": [5, 3], "seed": 9, "nextLevel": 2}},
                {"number": 2, "generate": {"pieces": "rooms.json", "rooms": 4}}
            ]
        })",
        "test catalog",
        "levels");

    const advanced_platformer::LevelCatalogEntry& first =
        advanced_platformer::levelEntry(catalog, 1);
    REQUIRE(first.generation.has_value());
    const advanced_platformer::LevelGeneration one =
        first.generation.value_or(advanced_platformer::LevelGeneration{});
    REQUIRE(one.relativePieces == std::filesystem::path("rooms.json"));
    REQUIRE(one.roomCount == 6);
    REQUIRE(one.grid.width == 5);
    REQUIRE(one.grid.height == 3);
    REQUIRE(one.seed == 9);
    REQUIRE(one.nextLevel == 2);
    const advanced_platformer::LevelCatalogEntry& second =
        advanced_platformer::levelEntry(catalog, 2);
    REQUIRE(second.generation.has_value());
    const advanced_platformer::LevelGeneration two =
        second.generation.value_or(advanced_platformer::LevelGeneration{});
    REQUIRE(two.grid.width == 9);
    REQUIRE(two.grid.height == 7);
    REQUIRE(two.seed == 2);
    REQUIRE_FALSE(two.nextLevel.has_value());
    REQUIRE_THROWS_WITH(
        advanced_platformer::levelPath(catalog, 1),
        Catch::Matchers::ContainsSubstring("Level 1 is generated"));
}

TEST_CASE("A level catalog rejects a generated entry it cannot build", "[app][content][json]")
{
    auto levelCatalogJson = tests::parseJson(
        R"({"startLevel":1,"cameraDeadZone":[80,45],"levels":[{"number":1,"generate":{"pieces":"rooms.json","rooms":4}}]})");
    auto& entry = levelCatalogJson["levels"][0];
    std::string message;
    SECTION("Both a file and a generation")
    {
        entry["file"] = "one.json";
        message = "levels[0]: expected exactly one of file and generate";
    }
    SECTION("Neither a file nor a generation")
    {
        tests::eraseKey(entry, "generate");
        message = "levels[0]: expected exactly one of file and generate";
    }
    SECTION("Too few rooms")
    {
        entry["generate"]["rooms"] = 1;
        message = "levels[0].generate.rooms: expected at least 2 rooms";
    }
    SECTION("More rooms than the grid holds")
    {
        entry["generate"]["grid"] = tests::numbers({2, 2});
        entry["generate"]["rooms"] = 5;
        message = "no more than the grid's 4 slots";
    }
    SECTION("A piece file outside the level directory")
    {
        entry["generate"]["pieces"] = "../rooms.json";
        message = "levels[0].generate.pieces: file must stay inside the level directory";
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelCatalog(tests::dumpJson(levelCatalogJson), "levels.json"),
        Catch::Matchers::ContainsSubstring(message));
}
