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
        // The seed a generated level was built from. Unset for a level read from a file.
        std::optional<std::uint32_t> seed;
    };

    // A catalog level's placements, read from its file or generated from its room pieces.
    struct CatalogLevel
    {
        LevelData data;
        // What diagnostics name: the file, or the room pieces with the level and seed.
        std::string sourceName;
        std::optional<std::uint32_t> seed;
    };

    // A seed replaces a generated level's own, as a reroll does. A level read from a file
    // ignores it.
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
    GameLevel composeStartedLevel(
        const LevelCatalog& catalog,
        int levelNumber,
        int textureId,
        const GameCatalogs& catalogs,
        Actor player,
        std::optional<std::uint32_t> seed = std::nullopt);
}
