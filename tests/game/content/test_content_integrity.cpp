#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "content/game_catalogs.hpp"
#include "content/item_catalog.hpp"
#include "content/pickup_catalog.hpp"
#include "content/tile_catalog.hpp"
#include "advanced_platformer/level/level_generator.hpp"
#include "advanced_platformer/level/room_pieces.hpp"
#include "content/room_pieces.hpp"
#include "content/npc_script_catalog.hpp"
#include "level/level_composition.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/level/placements.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_activity_scripts.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/world/level_validation.hpp"
#include "advanced_platformer/world/pickup.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "lua_npc_scripts.hpp"
#include "lua_presentation_script.hpp"
#include "support/fixed_step.hpp"

namespace
{
    int levelsUntilCap(const advanced_platformer::RunSettings& run)
    {
        if (run.roomsPerLevel <= 0 || run.firstRooms >= run.maxRooms)
        {
            return 1;
        }
        const int growth = run.maxRooms - run.firstRooms;
        return 1 + ((growth + run.roomsPerLevel - 1) / run.roomsPerLevel);
    }
}

TEST_CASE("Every run level can be composed until the rooms stop growing", "[app][content]")
{
    const auto pieces = advanced_platformer::loadRoomPieceCatalog("assets/catalogs/pieces.json");
    const auto catalogs = advanced_platformer::loadGameCatalogs("assets/catalogs");
    for (int number = 1; number <= levelsUntilCap(pieces.run); ++number)
    {
        const auto content = advanced_platformer::composeLevel(
            pieces,
            number,
            advanced_platformer::runLevelSeed(1, number),
            0,
            catalogs,
            advanced_platformer::composePlayer(catalogs, 0));
        INFO("Level " << number);
        REQUIRE(content.number == number);
        REQUIRE(content.world.exit().has_value());
    }
}

TEST_CASE(
    "Every shipped room piece places its actors and pickups where they can be",
    "[app][content]")
{
    REQUIRE_NOTHROW(
        advanced_platformer::validateRoomPieces(
            advanced_platformer::loadGameCatalogs("assets/catalogs"), tests::FixedStepSeconds));
}

namespace
{
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

    std::vector<advanced_platformer::Cell> openEdgeCells(
        const advanced_platformer::RoomPiece& piece,
        const advanced_platformer::TileMap& map)
    {
        const int width = static_cast<int>(piece.rows.front().size());
        const int height = static_cast<int>(piece.rows.size());
        std::vector<advanced_platformer::Cell> cells;
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
        std::erase_if(
            cells, [&](advanced_platformer::Cell cell) { return map.blocksMovement(cell); });
        return cells;
    }

    glm::vec2 playerLandingFeet(
        const advanced_platformer::TileMap& map,
        advanced_platformer::Actor player,
        advanced_platformer::Cell cell)
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
        const advanced_platformer::RoomPiece& piece,
        const advanced_platformer::TileMap& map,
        const advanced_platformer::Actor& player)
    {
        std::vector<glm::vec2> starts;
        if (piece.playerSpawn.has_value())
        {
            starts.push_back(advanced_platformer::feetInCell(map.tileSize(), *piece.playerSpawn));
        }
        for (const advanced_platformer::Cell cell : openEdgeCells(piece, map))
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
        const advanced_platformer::TileMap& map,
        advanced_platformer::Actor player,
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
        const advanced_platformer::TileMap& map,
        const advanced_platformer::Pickup& pickup,
        const advanced_platformer::GameCatalogs& catalogs)
    {
        const advanced_platformer::Cell cell = advanced_platformer::cellAtFeet(
            map.tileSize(), advanced_platformer::feetOf(pickup.body.bounds));
        advanced_platformer::TileMap broken = map;
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

    bool playerReachesPickup(
        const advanced_platformer::RoomPiece& piece,
        const advanced_platformer::PickupPlacement& placement,
        const advanced_platformer::GameCatalogs& catalogs)
    {
        const advanced_platformer::TileMap map = advanced_platformer::composeTileMap(
            piece.rows, catalogs.pieces.tileLegend, catalogs.tiles);
        const advanced_platformer::Actor player = advanced_platformer::composePlayer(catalogs, 0);
        const std::vector<glm::vec2> starts = playerStarts(piece, map, player);
        const advanced_platformer::Pickup pickup = advanced_platformer::composePickup(
            advanced_platformer::pickupDefinition(catalogs.pickups, placement.definitionName),
            catalogs.items,
            0,
            advanced_platformer::feetInCell(map.tileSize(), placement.spawn));
        if (playerReaches(map, player, starts, advanced_platformer::feetOf(pickup.body.bounds)))
        {
            return true;
        }
        const std::optional<glm::vec2> landing =
            landingFeetOnceTileBelowBreaks(map, pickup, catalogs);
        return landing.has_value() && playerReaches(map, player, starts, *landing);
    }

    advanced_platformer::RoomPiece mirrored(advanced_platformer::RoomPiece piece)
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
        return piece;
    }
}

TEST_CASE(
    "Every shipped room piece puts its pickups within the player's reach",
    "[app][content][generation]")
{
    const advanced_platformer::GameCatalogs catalogs =
        advanced_platformer::loadGameCatalogs("assets/catalogs");
    for (const advanced_platformer::RoomPiece& shipped : catalogs.pieces.pieces)
    {
        for (const bool flipped : {false, true})
        {
            if (flipped && !shipped.mirror)
            {
                continue;
            }
            const advanced_platformer::RoomPiece piece = flipped ? mirrored(shipped) : shipped;
            for (const advanced_platformer::PickupPlacement& placement : piece.pickups)
            {
                INFO(shipped.name << (flipped ? " (mirrored)" : "") << ": " << placement.id);
                CHECK(playerReachesPickup(piece, placement, catalogs));
            }
        }
    }
}

TEST_CASE("Every shipped Lua activity resolves", "[app][content][lua]")
{
    const advanced_platformer::GameCatalogs catalogs =
        advanced_platformer::loadGameCatalogs("assets/catalogs");
    advanced_platformer::LuaNpcScripts scripts;

    REQUIRE_NOTHROW(
        advanced_platformer::loadNpcActivityScripts(scripts, catalogs.machines, "assets/scripts"));
}

TEST_CASE("Every shipped Lua activity runs without errors", "[app][content][lua]")
{
    const advanced_platformer::GameCatalogs catalogs =
        advanced_platformer::loadGameCatalogs("assets/catalogs");
    advanced_platformer::LuaNpcScripts scripts;
    advanced_platformer::loadNpcActivityScripts(scripts, catalogs.machines, "assets/scripts");

    std::vector<advanced_platformer::NpcActivitySnapshot> situations(4);
    for (advanced_platformer::NpcActivitySnapshot& snapshot : situations)
    {
        snapshot.feet = {40.0F, 80.0F};
        snapshot.facts.primaryReady = true;
    }
    situations[0].targetFeet = {{60.0F, 80.0F}};
    situations[0].patrol = advanced_platformer::Patrol{{24.0F, 80.0F}, {96.0F, 80.0F}, true};
    situations[0].facts.targetKnown = true;
    situations[0].facts.targetVisible = true;
    situations[0].facts.targetInPrimaryRange = true;
    situations[1].patrol = situations[0].patrol;
    situations[2].routeComplete = true;
    situations[2].patrol = situations[0].patrol;
    situations[3].facts.primaryReady = false;
    situations[3].targetFeet = situations[0].targetFeet;

    std::uint32_t nextActor = 1;
    for (const auto& [machineName, machine] : catalogs.machines)
    {
        for (const advanced_platformer::NpcMachineState& state : machine.states)
        {
            for (const advanced_platformer::NpcActivitySnapshot& snapshot : situations)
            {
                const advanced_platformer::ActorId actor{nextActor++};
                scripts.enter(actor, state.does, snapshot);
                scripts.update(actor, state.does, snapshot, 1.0F / 120.0F);
                scripts.exit(actor, state.does, snapshot);
            }
            INFO(machineName << " " << state.name);
            REQUIRE(scripts.diagnostics().empty());
        }
    }
}

TEST_CASE("The shipped presentation script loads", "[app][content][lua]")
{
    advanced_platformer::LuaPresentationScript presentation;

    REQUIRE_NOTHROW(presentation.loadScript("assets/scripts/presentation.lua"));
    REQUIRE(presentation.loaded());
}
