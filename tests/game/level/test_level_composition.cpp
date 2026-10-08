#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <stdexcept>

#include "level/level_composition.hpp"
#include "content/game_catalogs.hpp"
#include "content/room_pieces.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/world/level_exit.hpp"
#include "advanced_platformer/world/pickup.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "advanced_platformer/level/placements.hpp"
#include "advanced_platformer/level/room_pieces.hpp"
#include "support/actor_components.hpp"
#include "support/fixed_step.hpp"

TEST_CASE("A level's cells become the feet of those cells on its map", "[app][content]")
{
    const auto pieces =
        advanced_platformer::loadRoomPieceCatalog("tests/fixtures/rooms/opening/pieces.json");
    const auto gameCatalogs = advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs");
    const auto gameLevel = advanced_platformer::composeLevel(
        pieces, 1, 1, 0, gameCatalogs, advanced_platformer::composePlayer(gameCatalogs, 0));
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
        advanced_platformer::loadRoomPieceCatalog("tests/fixtures/catalogs/pieces.json");
    const auto gameCatalogs = advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs");
    auto gameLevel = advanced_platformer::composeLevel(
        pieces, 1, 1, 0, gameCatalogs, advanced_platformer::composePlayer(gameCatalogs, 0));
    REQUIRE(gameLevel.world.actors().size() == 2);
    auto& actor = gameLevel.world.actors().front();
    REQUIRE(
        tests::component<advanced_platformer::PlatformerMovement>(actor).config.maximumSpeed == 23);
    REQUIRE(advanced_platformer::feetOf(actor.body.bounds).x == 152);
}

TEST_CASE("Level composition reports unknown actor definitions", "[app][actors]")
{
    const auto pieces =
        advanced_platformer::loadRoomPieceCatalog("tests/fixtures/rooms/unknown_actor/pieces.json");
    const auto gameCatalogs = advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs");
    REQUIRE_THROWS_WITH(
        advanced_platformer::composeLevel(
            pieces, 1, 1, 0, gameCatalogs, advanced_platformer::composePlayer(gameCatalogs, 0)),
        Catch::Matchers::ContainsSubstring("Level 1 (seed 1): actor '") &&
            Catch::Matchers::ContainsSubstring("unknown actor definition 'missing'"));
}

namespace
{
    advanced_platformer::GameCatalogs patrolPlacementCatalogs()
    {
        auto catalogs = advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs");
        catalogs.pieces = advanced_platformer::loadRoomPieceCatalog(
            "tests/fixtures/rooms/patrol_placement/pieces.json");
        return catalogs;
    }

    advanced_platformer::RoomPiece& startPiece(advanced_platformer::GameCatalogs& catalogs)
    {
        for (auto& piece : catalogs.pieces.pieces)
        {
            if (piece.name == "start")
            {
                return piece;
            }
        }
        throw std::logic_error("The patrol placement fixture must have a start piece");
    }
}

TEST_CASE(
    "Room pieces pass when every placement stands clear on reachable ground",
    "[app][generation]")
{
    REQUIRE_NOTHROW(
        advanced_platformer::validateRoomPieces(
            patrolPlacementCatalogs(), tests::FixedStepSeconds));
}

TEST_CASE("A room piece rejects a patrol its NPC cannot reach", "[app][generation]")
{
    auto catalogs = patrolPlacementCatalogs();
    startPiece(catalogs).actors.front().patrol =
        advanced_platformer::PatrolPlacement{{1, 4}, {6, 4}};

    REQUIRE_THROWS_WITH(
        advanced_platformer::validateRoomPieces(catalogs, tests::FixedStepSeconds),
        "Room piece 'start': actor 'test_guard_1': cannot reach its second patrol point");
}

TEST_CASE("A room piece rejects placements inside its walls", "[app][generation]")
{
    auto catalogs = patrolPlacementCatalogs();
    advanced_platformer::RoomPiece& start = startPiece(catalogs);

    SECTION("actor")
    {
        start.actors.front().spawn = {5, 4};
        REQUIRE_THROWS_WITH(
            advanced_platformer::validateRoomPieces(catalogs, tests::FixedStepSeconds),
            "Room piece 'start': actor 'test_guard_1': spawn overlaps a blocked tile");
    }

    SECTION("pickup")
    {
        start.pickups.push_back({"medicine_box_1", "medicine_box", {5, 3}});
        REQUIRE_THROWS_WITH(
            advanced_platformer::validateRoomPieces(catalogs, tests::FixedStepSeconds),
            "Room piece 'start': pickup 'medicine_box_1': spawn overlaps a blocked tile");
    }
}
