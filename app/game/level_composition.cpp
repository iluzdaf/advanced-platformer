#include "level_composition.hpp"
#include "content/game_catalogs.hpp"
#include "content/actor_catalog.hpp"
#include "content/actor_definition.hpp"
#include "content/item_catalog.hpp"
#include "content/pickup_catalog.hpp"
#include "content/exit_catalog.hpp"
#include "content/level_catalog.hpp"
#include "content/level_data.hpp"
#include "content/level_generator.hpp"
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
#include <variant>
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
        glm::vec2 feetOf(const TileMap& map, const LevelPosition& position)
        {
            if (const auto* cell = std::get_if<Cell>(&position))
            {
                return feetInCell(map.tileSize(), *cell);
            }
            return std::get<glm::vec2>(position);
        }

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
                feetOf(map, placement.spawn));
        }

        std::optional<Patrol> makePatrol(
            const TileMap& map,
            const std::optional<PatrolPlacement>& placement)
        {
            if (!placement.has_value())
            {
                return std::nullopt;
            }
            return Patrol{feetOf(map, placement->first), feetOf(map, placement->second), true};
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
                feetOf(map, placement.spawn));
            if (placement.requirement)
            {
                exit.requirement = composeItemStack(items, *placement.requirement);
            }
            exit.consumeItem = placement.consumeItem;
            exit.nextLevel = placement.nextLevel;
            return exit;
        }
    }

    CatalogLevel loadCatalogLevel(
        const LevelCatalog& catalog,
        int levelNumber,
        std::optional<std::uint32_t> seed)
    {
        LevelGeneration generation = levelEntry(catalog, levelNumber).generation;
        generation.seed = seed.value_or(generation.seed);
        const auto piecesPath = catalog.levelDirectory / generation.relativePieces;
        std::string sourceName = std::format(
            "{} (level {}, seed {})", piecesPath.string(), levelNumber, generation.seed);
        LevelData data = generateLevel(loadRoomPieceCatalog(piecesPath), generation, sourceName);
        return {std::move(data), std::move(sourceName), generation.seed};
    }

    GameLevel composeGameLevel(
        const LevelCatalog& catalog,
        int levelNumber,
        int textureId,
        const GameCatalogs& catalogs,
        std::optional<std::uint32_t> seed)
    {
        const CatalogLevel source = loadCatalogLevel(catalog, levelNumber, seed);
        const LevelData& data = source.data;
        const std::string& path = source.sourceName;
        const auto& tiles = catalogs.tiles;
        const auto& actors = catalogs.actors;
        const auto& exits = catalogs.exits;
        const auto& items = catalogs.items;
        const auto& pickups = catalogs.pickups;
        for (const auto& reference : data.exitReferences)
        {
            try
            {
                exitDefinition(exits, reference.second);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(
                    std::format("{}: {}: {}", path, reference.first, error.what()));
            }
        }
        for (const auto& reference : data.itemReferences)
        {
            try
            {
                itemDefinition(items, reference.second);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(
                    std::format("{}: {}: {}", path, reference.first, error.what()));
            }
        }
        for (const auto& reference : data.pickupReferences)
        {
            try
            {
                pickupDefinition(pickups, reference.second);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(
                    std::format("{}: {}: {}", path, reference.first, error.what()));
            }
        }
        for (const auto& reference : data.actorReferences)
        {
            try
            {
                actorDefinition(actors, reference.second);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(
                    std::format("{}: {}: {}", path, reference.first, error.what()));
            }
        }
        TileMap map = composeTileMap(data.mapRows, data.tileLegend, tiles);
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
                    feetOf(map, placement.spawn),
                    makePatrol(map, placement.patrol),
                    catalogs.machines));
                actorDefinitionNames.emplace(id.value, placement.definitionName);
                actorPlacementIds.emplace(id.value, placement.id);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(
                    std::format(
                        "{}: actor '{}': {}", path, placement.definitionName, error.what()));
            }
        }
        for (const auto& placement : data.pickups)
        {
            Pickup pickup = makePickup(map, placement, pickups, items, textureId);
            pickup.placement = pickupPlacementIds.size();
            pickupPlacementIds.push_back(placement.id);
            world.addPickup(pickup);
        }
        for (const auto& [actor, id] : actorPlacementIds)
        {
            placedIds.insert(id);
        }
        placedIds.insert(pickupPlacementIds.begin(), pickupPlacementIds.end());
        world.setExit(makeExit(map, textureId, data.exit, items, exits));
        const glm::vec2 playerSpawnFeet = feetOf(map, data.playerSpawn);
        return {
            levelNumber,
            std::move(map),
            std::move(world),
            playerSpawnFeet,
            std::move(actorDefinitionNames),
            std::move(actorPlacementIds),
            std::move(pickupPlacementIds),
            std::move(placedIds),
            source.seed};
    }

    Actor composePlayer(const GameCatalogs& catalogs, int textureId)
    {
        const auto& actors = catalogs.actors;
        return composeActor(actorDefinition(actors, actors.player), catalogs.animations, textureId);
    }

    namespace
    {
        GameLevel composeWithPlayer(
            const LevelCatalog& catalog,
            int levelNumber,
            int textureId,
            const GameCatalogs& catalogs,
            const Actor& player,
            std::optional<std::uint32_t> seed)
        {
            GameLevel level = composeGameLevel(catalog, levelNumber, textureId, catalogs, seed);
            Actor placed = player;
            moveFeetTo(placed.body.bounds, level.playerSpawnFeet);
            const ActorId playerId = level.world.addActor(std::move(placed));
            level.actorDefinitionNames.emplace(playerId.value, catalogs.actors.player);
            level.world.setPlayer(playerId, level.playerSpawnFeet);
            validateLevelActors(level.map, level.world, level.number);
            return level;
        }
    }

    GameLevel composeStartedLevel(
        const LevelCatalog& catalog,
        int levelNumber,
        int textureId,
        const GameCatalogs& catalogs,
        const Actor& player,
        float stepSeconds,
        std::optional<std::uint32_t> seed)
    {
        GameLevel level =
            composeWithPlayer(catalog, levelNumber, textureId, catalogs, player, seed);
        const std::uint32_t firstSeed = level.seed;
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
            level = composeWithPlayer(catalog, levelNumber, textureId, catalogs, player, nextSeed);
        }
        return level;
    }
}
