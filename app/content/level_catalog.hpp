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
    // How to build a level from room pieces instead of reading it from a file.
    struct LevelGeneration
    {
        // The room piece file, relative to the level directory.
        std::filesystem::path relativePieces;
        int roomCount = 0;
        // The grid of room slots the rooms grow in, from its centre.
        GridSize grid;
        // The same seed always builds the same level.
        std::uint32_t seed = 0;
        std::optional<int> nextLevel;
    };

    // A level comes from exactly one of a file or a generation.
    struct LevelCatalogEntry
    {
        int number = 0;
        std::filesystem::path relativeFile;
        std::optional<LevelGeneration> generation;
    };

    struct LevelCatalog
    {
        int startLevel = 0;
        // The part of the view the player moves in before the camera follows, in internal
        // pixels. It fits inside the view.
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
    // The file of a level read from one. A generated level has none.
    std::filesystem::path levelPath(const LevelCatalog& catalog, int levelNumber);
}
