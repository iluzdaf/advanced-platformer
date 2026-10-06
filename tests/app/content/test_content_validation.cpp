#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <limits>
#include <stdexcept>

#include "content/content_validation.hpp"
#include "content/item_catalog.hpp"
#include "content/tile_catalog.hpp"
#include "content/level_data.hpp"

TEST_CASE("Exit settings are validated without JSON", "[app][content][validation]")
{
    advanced_platformer::ExitPlacement exit;
    exit.definitionName = "test_door";
    REQUIRE_NOTHROW(advanced_platformer::validateExitSettings(exit));
    exit.requirement = advanced_platformer::NamedItemStack{"key", 1};
    REQUIRE_NOTHROW(advanced_platformer::validateExitSettings(exit));
    exit.requirement->quantity = 0;
    REQUIRE_THROWS_WITH(
        advanced_platformer::validateExitSettings(exit),
        "exit.requirement.quantity: expected a positive integer, got 0");
    exit.requirement->quantity = -1;
    REQUIRE_THROWS_AS(advanced_platformer::validateExitSettings(exit), std::invalid_argument);
}

TEST_CASE(
    "Tile catalog validation accepts C++ definitions without JSON",
    "[app][content][validation]")
{
    const advanced_platformer::TileCatalog catalog{
        16, {{false, false, {}}, {true, false, {{0, 0}, {16, 16}}}}, {{"empty", 0}, {"glass", 1}}};
    REQUIRE_NOTHROW(advanced_platformer::validateTileCatalog(catalog));
    REQUIRE_NOTHROW(
        advanced_platformer::validateTileLegend({{'.', "empty"}, {'X', "glass"}}, catalog));
    REQUIRE_THROWS_AS(
        advanced_platformer::validateTileLegend({{'?', "missing"}}, catalog),
        std::invalid_argument);
}

TEST_CASE("Tile catalog validation rejects invalid C++ definitions", "[app][content][validation]")
{
    advanced_platformer::TileCatalog catalog{
        16, {{false, false, {}}, {true, false, {{0, 0}, {16, 16}}}}, {{"empty", 0}, {"glass", 1}}};
    SECTION("Missing empty")
    {
        catalog.ids.erase("empty");
    }
    SECTION("Empty has wrong ID")
    {
        catalog.ids["empty"] = 1;
    }
    SECTION("Empty blocks movement")
    {
        catalog.definitions[0].blocksMovement = true;
    }
    SECTION("Empty blocks sight")
    {
        catalog.definitions[0].blocksSight = true;
    }
    SECTION("Empty is climbable")
    {
        catalog.definitions[0].climbable = true;
    }
    SECTION("Nonblocking tile is climbable")
    {
        catalog.definitions[1].blocksMovement = false;
        catalog.definitions[1].climbable = true;
    }
    SECTION("Negative ID")
    {
        catalog.ids["glass"] = -1;
    }
    SECTION("Out of range ID")
    {
        catalog.ids["glass"] = 2;
    }
    SECTION("Repeated ID")
    {
        catalog.ids["alias"] = 1;
    }
    SECTION("Unnamed definition")
    {
        catalog.ids.erase("glass");
    }
    SECTION("Negative sprite position")
    {
        catalog.definitions[1].sprite.position.x = -1;
    }
    SECTION("Zero sprite size")
    {
        catalog.definitions[1].sprite.size.x = 0;
    }
    SECTION("Nonfinite sprite")
    {
        catalog.definitions[1].sprite.size.y = std::numeric_limits<float>::infinity();
    }
    SECTION("Zero tile size")
    {
        catalog.tileSize = 0;
    }
    SECTION("Sprite is not one tile")
    {
        catalog.definitions[1].sprite.size = {16, 8};
    }
    REQUIRE_THROWS_AS(advanced_platformer::validateTileCatalog(catalog), std::invalid_argument);
}

TEST_CASE("Tile legend symbols are unambiguous independently of JSON", "[app][content][validation]")
{
    REQUIRE_NOTHROW(advanced_platformer::validateLegendSymbols({".", "#"}));
    REQUIRE_THROWS_AS(advanced_platformer::validateLegendSymbols({"long"}), std::invalid_argument);
    REQUIRE_THROWS_AS(advanced_platformer::validateLegendSymbols({""}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        advanced_platformer::validateLegendSymbols({".", "."}), std::invalid_argument);
}

TEST_CASE(
    "Map authoring validation reports useful paths without JSON",
    "[app][content][validation]")
{
    REQUIRE_NOTHROW(advanced_platformer::validateMapRows({"..", ".."}, {{'.', "empty"}}));
    REQUIRE_THROWS_WITH(
        advanced_platformer::validateMapRows({}, {{'.', "empty"}}),
        "map: expected at least one row");
    REQUIRE_THROWS_WITH(
        advanced_platformer::validateMapRows({""}, {{'.', "empty"}}),
        "map[0]: row cannot be empty");
    REQUIRE_THROWS_WITH(
        advanced_platformer::validateMapRows({"..", "."}, {{'.', "empty"}}),
        "map[1]: expected 2 columns, got 1");
    REQUIRE_THROWS_WITH(
        advanced_platformer::validateMapRows({".?"}, {{'.', "empty"}}),
        "map[0][1]: unknown symbol '?'; define it in tileLegend");
}
