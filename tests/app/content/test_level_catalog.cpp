#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <stdexcept>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <glm/vec2.hpp>
#include "content/level_catalog.hpp"
#include "support/json_document.hpp"

TEST_CASE("A level catalog lists generated levels by stable IDs", "[app][content][json]")
{
    const auto catalog = advanced_platformer::parseLevelCatalog(
        R"({
            "startLevel": 10,
            "cameraDeadZone": [80, 45],
            "levels": [
                {"number": 10, "pieces": "areas/rooms.json", "rooms": 6, "grid": [5, 3], "seed": 9, "nextLevel": 25},
                {"number": 25, "pieces": "rooms.json", "rooms": 4}
            ]
        })",
        "test catalog",
        "levels");

    REQUIRE(catalog.startLevel == 10);
    REQUIRE(catalog.cameraDeadZone == glm::vec2{80.0F, 45.0F});
    REQUIRE(catalog.levelDirectory == std::filesystem::path("levels"));
    REQUIRE(catalog.levels.size() == 2);
    const advanced_platformer::LevelGeneration& first =
        advanced_platformer::levelEntry(catalog, 10).generation;
    REQUIRE(first.relativePieces == std::filesystem::path("areas/rooms.json"));
    REQUIRE(first.roomCount == 6);
    REQUIRE(first.grid.width == 5);
    REQUIRE(first.grid.height == 3);
    REQUIRE(first.seed == 9);
    REQUIRE(first.nextLevel == 25);
    REQUIRE_THROWS_AS(advanced_platformer::levelEntry(catalog, 1), std::invalid_argument);
}

TEST_CASE("A level's grid and seed have defaults", "[app][content][json]")
{
    const auto catalog = advanced_platformer::parseLevelCatalog(
        R"({"startLevel": 3, "cameraDeadZone": [80, 45], "levels": [{"number": 3, "pieces": "rooms.json", "rooms": 4}]})",
        "test catalog");

    const advanced_platformer::LevelGeneration& level =
        advanced_platformer::levelEntry(catalog, 3).generation;
    REQUIRE(level.grid.width == 9);
    REQUIRE(level.grid.height == 7);
    REQUIRE(level.seed == 3);
    REQUIRE_FALSE(level.nextLevel.has_value());
}

TEST_CASE("A level catalog rejects entries it cannot build", "[app][content][json]")
{
    auto levelCatalogJson = tests::parseJson(
        R"({"startLevel":1,"cameraDeadZone":[80,45],"levels":[{"number":1,"pieces":"rooms.json","rooms":4}]})");
    auto& entry = levelCatalogJson["levels"][0];
    std::string message;
    SECTION("A start level that is not listed")
    {
        levelCatalogJson["startLevel"] = 2;
        message = "startLevel: level is not listed in the catalog";
    }
    SECTION("A repeated level number")
    {
        levelCatalogJson["levels"].get_array().push_back(entry);
        message = "levels[1].number: level number is already listed";
    }
    SECTION("No pieces")
    {
        tests::eraseKey(entry, "pieces");
        message = "levels.json:";
    }
    SECTION("Too few rooms")
    {
        entry["rooms"] = 1;
        message = "levels[0].rooms: expected at least 2 rooms";
    }
    SECTION("More rooms than the grid holds")
    {
        entry["grid"] = tests::numbers({2, 2});
        entry["rooms"] = 5;
        message = "no more than the grid's 4 slots";
    }
    SECTION("A piece file outside the level directory")
    {
        entry["pieces"] = "../rooms.json";
        message = "levels[0].pieces: expected a path inside the level directory";
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelCatalog(tests::dumpJson(levelCatalogJson), "levels.json"),
        Catch::Matchers::ContainsSubstring(message));
}

TEST_CASE(
    "A level catalog's camera dead zone is positive and fits in the view",
    "[app][content][json]")
{
    auto levelCatalogJson = tests::parseJson(
        R"({"startLevel":1,"cameraDeadZone":[80,45],"levels":[{"number":1,"pieces":"one.json","rooms":2,"grid":[2,1]}]})");
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

TEST_CASE("Level catalog numbers reject narrowing and fields reject typos", "[app][content][json]")
{
    auto levelCatalogJson = tests::parseJson(
        R"({"startLevel":1,"cameraDeadZone":[80,45],"levels":[{"number":1,"pieces":"one.json","rooms":2,"grid":[2,1]}]})");
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
        levelCatalogJson["levels"][0]["piecess"] = "two.json";
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseLevelCatalog(tests::dumpJson(levelCatalogJson), "levels.json"),
        Catch::Matchers::ContainsSubstring("levels.json:"));
}

TEST_CASE("A missing level catalog is rejected at the file boundary", "[app][content][json]")
{
    REQUIRE_THROWS_AS(
        advanced_platformer::loadLevelCatalog("tests/fixtures/levels/does_not_exist.json"),
        std::invalid_argument);
}
