#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/math/coordinates.hpp"

namespace advanced_platformer
{
    struct LevelGeneration
    {
        std::filesystem::path relativePieces;
        int roomCount = 0;
        GridSize grid;
        std::uint32_t seed = 0;
        std::optional<int> nextLevel;
    };

    struct LevelCatalogEntry
    {
        int number = 0;
        std::filesystem::path relativeFile;
        std::optional<LevelGeneration> generation;
    };

    struct LevelCatalog
    {
        int startLevel = 0;
        glm::vec2 cameraDeadZone = {0.0F, 0.0F};
        std::filesystem::path levelDirectory;
        std::vector<LevelCatalogEntry> levels;
    };

    LevelCatalog parseLevelCatalog(
        std::string_view text,
        std::string_view sourceName,
        const std::filesystem::path& levelDirectory = {});
    LevelCatalog loadLevelCatalog(const std::filesystem::path& path);
    const LevelCatalogEntry& levelEntry(const LevelCatalog& catalog, int levelNumber);
    std::filesystem::path levelPath(const LevelCatalog& catalog, int levelNumber);
}
