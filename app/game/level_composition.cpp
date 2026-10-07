#include "level_composition.hpp"
#include "content/game_catalogs.hpp"
#include "content/actor_catalog.hpp"
#include "content/actor_definition.hpp"
#include "content/item_catalog.hpp"
#include "content/pickup_catalog.hpp"
#include "content/exit_catalog.hpp"
#include "content/placements.hpp"
#include "level_generator.hpp"
#include "content/room_pieces.hpp"
#include "content/tile_catalog.hpp"
#include <cstdint>
#include <format>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
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
        Pickup makePickup(
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

        std::optional<Patrol> makePatrol(
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

        LevelExit makeExit(
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

        struct GeneratedLevel
        {
            LevelData data;
            std::string sourceName;
        };

        GeneratedLevel generateRunLevel(
            const RoomPieceCatalog& pieces,
            int levelNumber,
            std::uint32_t seed)
        {
            const LevelGeneration generation = levelGeneration(pieces.run, levelNumber, seed);
            std::string sourceName = std::format("Level {} (seed {})", levelNumber, seed);
            LevelData data = generateLevel(pieces, generation, sourceName);
            return {std::move(data), std::move(sourceName)};
        }

        GameLevel composeGameLevel(
            const RoomPieceCatalog& pieces,
            int levelNumber,
            std::uint32_t seed,
            int textureId,
            const GameCatalogs& catalogs)
        {
            const GeneratedLevel source = generateRunLevel(pieces, levelNumber, seed);
            const LevelData& data = source.data;
            const std::string& path = source.sourceName;
            const auto& actors = catalogs.actors;
            const auto& exits = catalogs.exits;
            const auto& items = catalogs.items;
            const auto& pickups = catalogs.pickups;
            TileMap map = composeTileMap(data.mapRows, data.tileLegend, catalogs.tiles);
            World world(composeItems(items, textureId));
            std::unordered_map<std::uint32_t, std::string> actorDefinitionNames;
            std::unordered_map<std::uint32_t, std::string> actorPlacementIds;
            std::vector<std::string> pickupPlacementIds;
            std::set<std::string> placedIds;
            for (const auto& placement : data.actors)
            {
                try
                {
                    const ActorId id = world.addActor(composeActor(
                        actorDefinition(actors, placement.definitionName),
                        catalogs.animations,
                        textureId,
                        feetInCell(map.tileSize(), placement.spawn),
                        makePatrol(map, placement.patrol),
                        catalogs.machines));
                    actorDefinitionNames.emplace(id.value, placement.definitionName);
                    actorPlacementIds.emplace(id.value, placement.id);
                }
                catch (const std::invalid_argument& error)
                {
                    throw std::invalid_argument(
                        std::format("{}: actor '{}': {}", path, placement.id, error.what()));
                }
            }
            for (const auto& placement : data.pickups)
            {
                try
                {
                    Pickup pickup = makePickup(map, placement, pickups, items, textureId);
                    pickup.placement = pickupPlacementIds.size();
                    pickupPlacementIds.push_back(placement.id);
                    world.addPickup(pickup);
                }
                catch (const std::invalid_argument& error)
                {
                    throw std::invalid_argument(
                        std::format("{}: pickup '{}': {}", path, placement.id, error.what()));
                }
            }
            for (const auto& [actor, id] : actorPlacementIds)
            {
                placedIds.insert(id);
            }
            placedIds.insert(pickupPlacementIds.begin(), pickupPlacementIds.end());
            try
            {
                world.setExit(makeExit(map, textureId, data.exit, items, exits));
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(std::format("{}: exit: {}", path, error.what()));
            }
            const glm::vec2 playerSpawnFeet = feetInCell(map.tileSize(), data.playerSpawn);
            return {
                levelNumber,
                std::move(map),
                std::move(world),
                playerSpawnFeet,
                std::move(actorDefinitionNames),
                std::move(actorPlacementIds),
                std::move(pickupPlacementIds),
                std::move(placedIds),
                seed};
        }
    }

    Actor composePlayer(const GameCatalogs& catalogs, int textureId)
    {
        const auto& actors = catalogs.actors;
        return composeActor(actorDefinition(actors, actors.player), catalogs.animations, textureId);
    }

    GameLevel composeLevelAtSeed(
        const RoomPieceCatalog& pieces,
        int levelNumber,
        std::uint32_t seed,
        int textureId,
        const GameCatalogs& catalogs,
        const Actor& player)
    {
        GameLevel level = composeGameLevel(pieces, levelNumber, seed, textureId, catalogs);
        Actor placed = player;
        moveFeetTo(placed.body.bounds, level.playerSpawnFeet);
        const ActorId playerId = level.world.addActor(std::move(placed));
        level.actorDefinitionNames.emplace(playerId.value, catalogs.actors.player);
        level.world.setPlayer(playerId, level.playerSpawnFeet);
        validateLevelActors(level.map, level.world, level.number);
        return level;
    }

    namespace
    {
        constexpr std::uint32_t GenerationAttempts = 100;
    }

    GameLevel composeStartedLevel(
        const RoomPieceCatalog& pieces,
        int levelNumber,
        std::uint32_t seed,
        int textureId,
        const GameCatalogs& catalogs,
        const Actor& player,
        float stepSeconds)
    {
        GameLevel level =
            composeLevelAtSeed(pieces, levelNumber, seed, textureId, catalogs, player);
        const std::uint32_t firstSeed = seed;
        for (std::uint32_t nextSeed = firstSeed + 1U;
             !playerCanReachExit(level.map, level.world, stepSeconds);
             ++nextSeed)
        {
            if (nextSeed - firstSeed == GenerationAttempts)
            {
                throw std::invalid_argument(
                    std::format(
                        "Level {}: no seed from {} to {} gives a route from the spawn to the "
                        "exit",
                        levelNumber,
                        firstSeed,
                        nextSeed - 1U));
            }
            level = composeLevelAtSeed(pieces, levelNumber, nextSeed, textureId, catalogs, player);
        }
        return level;
    }
}
