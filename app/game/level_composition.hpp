#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "content/level_data.hpp"

namespace advanced_platformer
{
    struct Actor;
    struct LevelCatalog;
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

    struct CatalogLevel
    {
        LevelData data;
        std::string sourceName;
        std::uint32_t seed = 0;
    };

    CatalogLevel loadCatalogLevel(
        const LevelCatalog& catalog,
        int levelNumber,
        std::optional<std::uint32_t> seed = std::nullopt);
    GameLevel composeGameLevel(
        const LevelCatalog& catalog,
        int levelNumber,
        int textureId,
        const GameCatalogs& catalogs,
        std::optional<std::uint32_t> seed = std::nullopt);
    Actor composePlayer(const GameCatalogs& catalogs, int textureId);
    GameLevel composeLevelAtSeed(
        const LevelCatalog& catalog,
        int levelNumber,
        int textureId,
        const GameCatalogs& catalogs,
        const Actor& player,
        std::optional<std::uint32_t> seed = std::nullopt);
    GameLevel composeStartedLevel(
        const LevelCatalog& catalog,
        int levelNumber,
        int textureId,
        const GameCatalogs& catalogs,
        const Actor& player,
        float stepSeconds,
        std::optional<std::uint32_t> seed = std::nullopt);
}
