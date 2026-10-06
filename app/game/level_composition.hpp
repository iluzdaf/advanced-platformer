#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "level_generator.hpp"

namespace advanced_platformer
{
    struct Actor;
    struct RoomPieceCatalog;
    struct GameCatalogs;

    constexpr std::uint32_t GenerationAttempts = 100;

    struct GameLevel
    {
        int number = 0;
        TileMap map;
        World world;
        glm::vec2 playerSpawnFeet = {0.0F, 0.0F};
        std::unordered_map<std::uint32_t, std::string> actorDefinitionNames;
        std::unordered_map<std::uint32_t, std::string> actorPlacementIds;
        std::vector<std::string> pickupPlacementIds;
        std::set<std::string> placedIds;
        std::uint32_t seed = 0;
    };

    struct GeneratedLevel
    {
        LevelData data;
        std::string sourceName;
    };

    GeneratedLevel generateRunLevel(
        const RoomPieceCatalog& pieces,
        int levelNumber,
        std::uint32_t seed);
    GameLevel composeGameLevel(
        const RoomPieceCatalog& pieces,
        int levelNumber,
        std::uint32_t seed,
        int textureId,
        const GameCatalogs& catalogs);
    Actor composePlayer(const GameCatalogs& catalogs, int textureId);
    GameLevel composeLevelAtSeed(
        const RoomPieceCatalog& pieces,
        int levelNumber,
        std::uint32_t seed,
        int textureId,
        const GameCatalogs& catalogs,
        const Actor& player);
    GameLevel composeStartedLevel(
        const RoomPieceCatalog& pieces,
        int levelNumber,
        std::uint32_t seed,
        int textureId,
        const GameCatalogs& catalogs,
        const Actor& player,
        float stepSeconds);
}
