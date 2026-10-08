#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "content/game_catalogs.hpp"
#include "content/item_catalog.hpp"
#include "content/pickup_catalog.hpp"
#include "content/tile_catalog.hpp"
#include "level/level_composition.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/level/placements.hpp"
#include "advanced_platformer/level/room_pieces.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/world/level_validation.hpp"
#include "advanced_platformer/world/pickup.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "support/fixed_step.hpp"

namespace
{
    using advanced_platformer::Actor;
    using advanced_platformer::Cell;
    using advanced_platformer::GameCatalogs;
    using advanced_platformer::Pickup;
    using advanced_platformer::RoomPiece;
    using advanced_platformer::TileMap;
    constexpr float SettleSeconds = 10.0F;

    template <typename Step>
    void stepUntilAtRest(const advanced_platformer::Aabb& bounds, Step step)
    {
        const int steps = static_cast<int>(SettleSeconds / tests::FixedStepSeconds);
        for (int count = 0; count < steps; ++count)
        {
            const glm::vec2 before = advanced_platformer::feetOf(bounds);
            step();
            if (advanced_platformer::feetOf(bounds) == before)
            {
                return;
            }
        }
    }

    std::vector<Cell> openEdgeCells(const RoomPiece& piece, const TileMap& map)
    {
        const int width = static_cast<int>(piece.rows.front().size());
        const int height = static_cast<int>(piece.rows.size());
        std::vector<Cell> cells;
        for (int y = 0; y < height - 1; ++y)
        {
            cells.push_back({0, y});
            cells.push_back({width - 1, y});
        }
        for (int x = 1; x < width - 1; ++x)
        {
            cells.push_back({x, 0});
            cells.push_back({x, height - 1});
        }
        std::erase_if(cells, [&](Cell cell) { return map.blocksMovement(cell); });
        return cells;
    }

    glm::vec2 playerLandingFeet(const TileMap& map, Actor player, Cell cell)
    {
        advanced_platformer::moveFeetTo(
            player.body.bounds, advanced_platformer::feetInCell(map.tileSize(), cell));
        if (player.platformerMovement.has_value())
        {
            stepUntilAtRest(
                player.body.bounds,
                [&]
                {
                    advanced_platformer::updatePlatformerMovement(
                        map, player.body, *player.platformerMovement, {}, tests::FixedStepSeconds);
                });
        }
        return advanced_platformer::feetOf(player.body.bounds);
    }

    std::vector<glm::vec2> playerStarts(
        const RoomPiece& piece,
        const TileMap& map,
        const Actor& player)
    {
        std::vector<glm::vec2> starts;
        if (piece.playerSpawn.has_value())
        {
            starts.push_back(advanced_platformer::feetInCell(map.tileSize(), *piece.playerSpawn));
        }
        for (const Cell cell : openEdgeCells(piece, map))
        {
            const glm::vec2 feet = playerLandingFeet(map, player, cell);
            if (std::ranges::find(starts, feet) == starts.end())
            {
                starts.push_back(feet);
            }
        }
        return starts;
    }

    bool playerReaches(
        const TileMap& map,
        Actor player,
        const std::vector<glm::vec2>& starts,
        glm::vec2 goal)
    {
        return std::ranges::any_of(
            starts,
            [&](glm::vec2 start)
            {
                advanced_platformer::moveFeetTo(player.body.bounds, start);
                return advanced_platformer::actorCanReach(
                    map, player, goal, tests::FixedStepSeconds);
            });
    }

    std::optional<glm::vec2> landingFeetOnceTileBelowBreaks(
        const TileMap& map,
        const Pickup& pickup,
        const GameCatalogs& catalogs)
    {
        const Cell cell = advanced_platformer::cellAtFeet(
            map.tileSize(), advanced_platformer::feetOf(pickup.body.bounds));
        TileMap broken = map;
        if (!broken.breakTile({cell.x, cell.y + 1}))
        {
            return std::nullopt;
        }
        advanced_platformer::World world(advanced_platformer::composeItems(catalogs.items, 0));
        world.addPickup(pickup);
        const advanced_platformer::Aabb& bounds = world.pickups().front().body.bounds;
        stepUntilAtRest(
            bounds,
            [&]
            { advanced_platformer::updatePickupMovement(broken, world, tests::FixedStepSeconds); });
        return advanced_platformer::feetOf(bounds);
    }

    void checkPickupsReachable(const RoomPiece& piece, const GameCatalogs& catalogs)
    {
        const TileMap map = advanced_platformer::composeTileMap(
            piece.rows, catalogs.pieces.tileLegend, catalogs.tiles);
        const Actor player = advanced_platformer::composePlayer(catalogs, 0);
        const std::vector<glm::vec2> starts = playerStarts(piece, map, player);
        for (const advanced_platformer::PickupPlacement& placement : piece.pickups)
        {
            const Pickup pickup = advanced_platformer::composePickup(
                advanced_platformer::pickupDefinition(catalogs.pickups, placement.definitionName),
                catalogs.items,
                0,
                advanced_platformer::feetInCell(map.tileSize(), placement.spawn));
            const glm::vec2 feet = advanced_platformer::feetOf(pickup.body.bounds);
            const std::optional<glm::vec2> landing =
                landingFeetOnceTileBelowBreaks(map, pickup, catalogs);
            INFO("Room piece '" << piece.name << "': pickup '" << placement.id << "'");
            CHECK(
                (playerReaches(map, player, starts, feet) ||
                 (landing.has_value() && playerReaches(map, player, starts, *landing))));
        }
    }

    RoomPiece mirrored(RoomPiece piece)
    {
        const int width = static_cast<int>(piece.rows.front().size());
        for (std::string& row : piece.rows)
        {
            std::ranges::reverse(row);
        }
        if (piece.playerSpawn.has_value())
        {
            piece.playerSpawn->x = width - 1 - piece.playerSpawn->x;
        }
        for (advanced_platformer::PickupPlacement& pickup : piece.pickups)
        {
            pickup.spawn.x = width - 1 - pickup.spawn.x;
        }
        piece.name += " mirrored";
        return piece;
    }
}

TEST_CASE(
    "Every shipped room piece puts its pickups where the player can reach them from a door or "
    "the player spawn, or where they fall once the tile under them breaks",
    "[app][content][generation]")
{
    const GameCatalogs catalogs = advanced_platformer::loadGameCatalogs("assets/catalogs");
    for (const RoomPiece& piece : catalogs.pieces.pieces)
    {
        checkPickupsReachable(piece, catalogs);
        if (piece.mirror)
        {
            checkPickupsReachable(mirrored(piece), catalogs);
        }
    }
}
