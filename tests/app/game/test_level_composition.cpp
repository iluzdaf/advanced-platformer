#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <stdexcept>

#include "game/level_composition.hpp"
#include "content/game_catalogs.hpp"
#include "content/room_pieces.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/world/level_exit.hpp"
#include "advanced_platformer/world/pickup.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "support/actor_components.hpp"
#include "support/atlas_size.hpp"

TEST_CASE("A level's cells become the feet of those cells on its map", "[app][content]")
{
    const auto pieces =
        advanced_platformer::loadRoomPieceCatalog("tests/fixtures/levels/opening.json");
    const auto gameCatalogs =
        advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize);
    const auto gameLevel = advanced_platformer::composeGameLevel(pieces, 1, 1, 0, gameCatalogs);
    const int tileSize = gameLevel.map.tileSize();

    REQUIRE(gameLevel.playerSpawnFeet == advanced_platformer::feetInCell(tileSize, {12, 4}));
    REQUIRE(gameLevel.world.pickups().size() == 1);
    REQUIRE(
        advanced_platformer::feetOf(gameLevel.world.pickups().front().body.bounds) ==
        advanced_platformer::feetInCell(tileSize, {10, 4}));
    const auto& levelExit = gameLevel.world.exit();
    if (!levelExit.has_value())
    {
        throw std::logic_error("The opening level must have an exit");
    }
    REQUIRE(
        advanced_platformer::feetOf(levelExit->bounds) ==
        advanced_platformer::feetInCell(tileSize, {2, 4}));
}

TEST_CASE("A level composes an actor from its catalog definition", "[app][actors]")
{
    const auto pieces =
        advanced_platformer::loadRoomPieceCatalog("tests/fixtures/levels/actor_placement.json");
    const auto gameCatalogs =
        advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize);
    auto gameLevel = advanced_platformer::composeGameLevel(pieces, 1, 1, 0, gameCatalogs);
    REQUIRE(gameLevel.world.actors().size() == 1);
    auto& actor = gameLevel.world.actors().front();
    REQUIRE(
        tests::component<advanced_platformer::PlatformerMovement>(actor).config.maximumSpeed == 23);
    REQUIRE(advanced_platformer::feetOf(actor.body.bounds).x == 152);
}

TEST_CASE("Level composition reports unknown actor definitions", "[app][actors]")
{
    const auto pieces =
        advanced_platformer::loadRoomPieceCatalog("tests/fixtures/levels/unknown_actor.json");
    const auto gameCatalogs =
        advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize);
    REQUIRE_THROWS_WITH(
        advanced_platformer::composeGameLevel(pieces, 1, 1, 0, gameCatalogs),
        Catch::Matchers::ContainsSubstring("Level 1 (seed 1): actor '") &&
            Catch::Matchers::ContainsSubstring("unknown actor definition 'missing'"));
}
