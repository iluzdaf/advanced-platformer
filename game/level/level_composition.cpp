#include "level_composition.hpp"
#include "content/game_catalogs.hpp"
#include "content/actor_catalog.hpp"
#include "content/actor_definition.hpp"
#include "content/item_catalog.hpp"
#include "content/pickup_catalog.hpp"
#include "content/exit_catalog.hpp"
#include "advanced_platformer/level/placements.hpp"
#include "advanced_platformer/level/level_generator.hpp"
#include "advanced_platformer/level/room_pieces.hpp"
#include "content/tile_catalog.hpp"
#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/world/level_exit.hpp"
#include "advanced_platformer/world/pickup.hpp"
#include "advanced_platformer/world/level_validation.hpp"
#include "advanced_platformer/world/tile_map.hpp"

namespace advanced_platformer
{
    namespace
    {
        Pickup composePlacedPickup(
            const TileMap& map,
            const PickupPlacement& placement,
            const PickupCatalog& pickups,
            const ItemCatalog& items,
            int textureId)
        {
            return composePickup(
                pickupDefinition(pickups, placement.definitionName),
                items,
                textureId,
                feetInCell(map.tileSize(), placement.spawn));
        }

        std::optional<Patrol> composePatrol(
            const TileMap& map,
            const std::optional<PatrolPlacement>& placement)
        {
            if (!placement.has_value())
            {
                return std::nullopt;
            }
            return Patrol{
                feetInCell(map.tileSize(), placement->first),
                feetInCell(map.tileSize(), placement->second),
                true};
        }

        Actor composePlacedActor(
            const TileMap& map,
            const ActorPlacement& placement,
            const GameCatalogs& catalogs,
            int textureId)
        {
            return composeActor(
                actorDefinition(catalogs.actors, placement.definitionName),
                catalogs.animations,
                textureId,
                feetInCell(map.tileSize(), placement.spawn),
                composePatrol(map, placement.patrol),
                catalogs.machines);
        }

        LevelExit composePlacedExit(
            const TileMap& map,
            int textureId,
            const ExitPlacement& placement,
            const ItemCatalog& items,
            const ExitCatalog& exits)
        {
            LevelExit exit = composeExit(
                exitDefinition(exits, placement.definitionName),
                textureId,
                feetInCell(map.tileSize(), placement.spawn));
            if (placement.requirement)
            {
                exit.requirement = composeItemStack(items, *placement.requirement);
            }
            exit.consumeItem = placement.consumeItem;
            return exit;
        }

        GameLevel composeGeneratedLevel(
            const GeneratedLevel& generated,
            const std::string& levelName,
            int levelNumber,
            std::uint32_t seed,
            int textureId,
            const GameCatalogs& catalogs)
        {
            const auto& exits = catalogs.exits;
            const auto& items = catalogs.items;
            const auto& pickups = catalogs.pickups;
            TileMap map = composeTileMap(generated.mapRows, generated.tileLegend, catalogs.tiles);
            World world(composeItems(items, textureId));
            std::unordered_map<std::uint32_t, std::string> actorDefinitionNames;
            std::unordered_map<std::uint32_t, std::string> actorPlacementIds;
            std::vector<std::string> pickupPlacementIds;
            std::set<std::string> placedIds;
            for (const auto& placement : generated.actors)
            {
                try
                {
                    const ActorId id =
                        world.addActor(composePlacedActor(map, placement, catalogs, textureId));
                    actorDefinitionNames.emplace(id.value, placement.definitionName);
                    actorPlacementIds.emplace(id.value, placement.id);
                }
                catch (const std::invalid_argument& error)
                {
                    throw std::invalid_argument(
                        std::format("{}: actor '{}': {}", levelName, placement.id, error.what()));
                }
            }
            for (const auto& placement : generated.pickups)
            {
                try
                {
                    Pickup pickup = composePlacedPickup(map, placement, pickups, items, textureId);
                    pickup.placement = pickupPlacementIds.size();
                    pickupPlacementIds.push_back(placement.id);
                    world.addPickup(pickup);
                }
                catch (const std::invalid_argument& error)
                {
                    throw std::invalid_argument(
                        std::format("{}: pickup '{}': {}", levelName, placement.id, error.what()));
                }
            }
            for (const auto& [actor, id] : actorPlacementIds)
            {
                placedIds.insert(id);
            }
            placedIds.insert(pickupPlacementIds.begin(), pickupPlacementIds.end());
            try
            {
                world.setExit(composePlacedExit(map, textureId, generated.exit, items, exits));
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(std::format("{}: exit: {}", levelName, error.what()));
            }
            const glm::vec2 playerSpawnFeet = feetInCell(map.tileSize(), generated.playerSpawn);
            return {
                levelNumber,
                std::move(map),
                std::move(world),
                playerSpawnFeet,
                std::move(actorDefinitionNames),
                std::move(actorPlacementIds),
                std::move(pickupPlacementIds),
                std::move(placedIds),
                seed,
                generated.rooms};
        }
    }

    Actor composePlayer(const GameCatalogs& catalogs, int textureId)
    {
        const auto& actors = catalogs.actors;
        return composeActor(
            actorDefinition(actors, actors.player),
            catalogs.animations,
            textureId,
            {},
            std::nullopt,
            catalogs.machines);
    }

    GameLevel composeLevel(
        const RoomPieces& pieces,
        int levelNumber,
        std::uint32_t seed,
        int textureId,
        const GameCatalogs& catalogs,
        const Actor& player)
    {
        const std::string levelName = std::format("Level {} (seed {})", levelNumber, seed);
        const GeneratedLevel generated = generateLevel(pieces, levelNumber, seed, levelName);
        GameLevel level =
            composeGeneratedLevel(generated, levelName, levelNumber, seed, textureId, catalogs);
        Actor placed = player;
        moveFeetTo(placed.body.bounds, level.playerSpawnFeet);
        const ActorId playerId = level.world.addActor(std::move(placed));
        level.actorDefinitionNames.emplace(playerId.value, catalogs.actors.player);
        level.world.setPlayer(playerId, level.playerSpawnFeet);
        validateLevelPlacements(level.map, level.world, level.number);
        return level;
    }

    namespace
    {
        constexpr std::uint32_t SeedAttempts = 100;
    }

    GameLevel composePlayableLevel(
        const RoomPieces& pieces,
        int levelNumber,
        std::uint32_t seed,
        int textureId,
        const GameCatalogs& catalogs,
        const Actor& player,
        float stepSeconds)
    {
        GameLevel level = composeLevel(pieces, levelNumber, seed, textureId, catalogs, player);
        const std::uint32_t firstSeed = seed;
        for (std::uint32_t nextSeed = firstSeed + 1U;
             !playerCanReachExit(level.map, level.world, stepSeconds);
             ++nextSeed)
        {
            if (nextSeed - firstSeed == SeedAttempts)
            {
                throw std::invalid_argument(
                    std::format(
                        "Level {}: no seed from {} to {} gives a route from the spawn to the "
                        "exit",
                        levelNumber,
                        firstSeed,
                        nextSeed - 1U));
            }
            level = composeLevel(pieces, levelNumber, nextSeed, textureId, catalogs, player);
        }
        return level;
    }

    namespace
    {
        Cell mirroredCell(Cell cell, int width)
        {
            return {width - 1 - cell.x, cell.y};
        }

        RoomPiece mirroredPiece(const RoomPiece& piece, int width)
        {
            RoomPiece mirrored = piece;
            for (std::string& row : mirrored.rows)
            {
                std::ranges::reverse(row);
            }
            for (ActorPlacement& actor : mirrored.actors)
            {
                actor.spawn = mirroredCell(actor.spawn, width);
                if (actor.patrol.has_value())
                {
                    actor.patrol = PatrolPlacement{
                        mirroredCell(actor.patrol->first, width),
                        mirroredCell(actor.patrol->second, width)};
                }
            }
            for (PickupPlacement& pickup : mirrored.pickups)
            {
                pickup.spawn = mirroredCell(pickup.spawn, width);
            }
            return mirrored;
        }

        void requireReachable(
            const TileMap& map,
            const Actor& actor,
            glm::vec2 feet,
            std::string_view place,
            float stepSeconds)
        {
            if (!actorCanReach(map, actor, feet, stepSeconds))
            {
                throw std::invalid_argument(std::format("cannot reach its {}", place));
            }
        }

        void validatePieceOrientation(
            const RoomPiece& piece,
            const RoomPieces& pieces,
            const GameCatalogs& catalogs,
            float stepSeconds,
            std::string_view name)
        {
            const TileMap map = composeTileMap(piece.rows, pieces.tileLegend, catalogs.tiles);
            for (const ActorPlacement& placement : piece.actors)
            {
                try
                {
                    const Actor actor = composePlacedActor(map, placement, catalogs, 0);
                    validateActorPlacement(map, actor);
                    if (actor.patrol.has_value())
                    {
                        requireReachable(
                            map, actor, actor.patrol->firstFeet, "first patrol point", stepSeconds);
                        requireReachable(
                            map,
                            actor,
                            actor.patrol->secondFeet,
                            "second patrol point",
                            stepSeconds);
                    }
                }
                catch (const std::invalid_argument& error)
                {
                    throw std::invalid_argument(
                        std::format("{}: actor '{}': {}", name, placement.id, error.what()));
                }
            }
            for (const PickupPlacement& placement : piece.pickups)
            {
                try
                {
                    validatePickupPlacement(
                        map,
                        composePlacedPickup(map, placement, catalogs.pickups, catalogs.items, 0));
                }
                catch (const std::invalid_argument& error)
                {
                    throw std::invalid_argument(
                        std::format("{}: pickup '{}': {}", name, placement.id, error.what()));
                }
            }
        }
    }

    void validateRoomPieces(const GameCatalogs& catalogs, float stepSeconds)
    {
        const RoomPieces& pieces = catalogs.pieces;
        for (const RoomPiece& piece : pieces.pieces)
        {
            const std::string name = std::format("Room piece '{}'", piece.name);
            validatePieceOrientation(piece, pieces, catalogs, stepSeconds, name);
            if (piece.mirror)
            {
                validatePieceOrientation(
                    mirroredPiece(piece, pieces.roomSize.width),
                    pieces,
                    catalogs,
                    stepSeconds,
                    name + " mirrored");
            }
        }
    }
}
