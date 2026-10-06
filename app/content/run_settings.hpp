#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

#include "advanced_platformer/math/coordinates.hpp"
#include "level_generator.hpp"

namespace advanced_platformer
{
    struct RunSettings
    {
        std::filesystem::path levelDirectory;
        std::filesystem::path relativePieces;
        GridSize grid;
        int firstRooms = 0;
        int roomsPerLevel = 0;
        int maxRooms = 0;
    };

    RunSettings parseRunSettings(
        std::string_view text,
        std::string_view sourceName,
        const std::filesystem::path& levelDirectory = {});
    RunSettings loadRunSettings(const std::filesystem::path& path);

    int roomsForLevel(const RunSettings& run, int levelNumber);
    LevelGeneration levelGeneration(const RunSettings& run, int levelNumber, std::uint32_t seed);
    std::uint32_t runLevelSeed(std::uint32_t runSeed, int levelNumber);
    std::uint32_t nextRunSeed(std::uint32_t runSeed);
}
