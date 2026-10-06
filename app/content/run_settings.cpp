#include "run_settings.hpp"

#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "level_generator.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <glaze/glaze.hpp>

#include "advanced_platformer/math/coordinates.hpp"

namespace advanced_platformer
{
    struct RunSettingsJson
    {
        std::string pieces;
        std::optional<std::array<int, 2>> grid;
        int firstRooms = 0;
        int roomsPerLevel = 0;
        int maxRooms = 0;
    };

    namespace
    {
        constexpr std::array<int, 2> DefaultGenerationGrid = {9, 7};

        std::filesystem::path relativeFileFrom(
            const std::string& text,
            std::string_view sourceName,
            std::string_view path)
        {
            const std::filesystem::path file = text;
            if (file.empty() || file.is_absolute())
            {
                failJson(sourceName, path, "expected a non-empty relative path");
            }
            for (const std::filesystem::path& part : file)
            {
                if (part == "..")
                {
                    failJson(sourceName, path, "expected a path inside the level directory");
                }
            }
            return file;
        }
    }

    RunSettings parseRunSettings(
        std::string_view text,
        std::string_view sourceName,
        const std::filesystem::path& levelDirectory)
    {
        const auto file = readContent<RunSettingsJson>(text, sourceName);
        RunSettings result;
        result.levelDirectory = levelDirectory;
        result.relativePieces = relativeFileFrom(file.pieces, sourceName, "pieces");
        const std::array<int, 2> grid = file.grid.value_or(DefaultGenerationGrid);
        result.grid = {grid[0], grid[1]};
        if (result.grid.width <= 0 || result.grid.height <= 0)
        {
            failJson(sourceName, "grid", "expected a positive size");
        }
        const int slots = result.grid.width * result.grid.height;
        result.firstRooms = file.firstRooms;
        result.roomsPerLevel = file.roomsPerLevel;
        result.maxRooms = file.maxRooms;
        if (result.maxRooms < 2 || result.maxRooms > slots)
        {
            failJson(
                sourceName,
                "maxRooms",
                std::format(
                    "expected at least 2 rooms and no more than the grid's {} slots", slots));
        }
        if (result.firstRooms < 2 || result.firstRooms > result.maxRooms)
        {
            failJson(
                sourceName, "firstRooms", "expected at least 2 rooms and no more than maxRooms");
        }
        if (result.roomsPerLevel < 0)
        {
            failJson(sourceName, "roomsPerLevel", "expected zero or more rooms");
        }
        return result;
    }

    RunSettings loadRunSettings(const std::filesystem::path& path)
    {
        return parseRunSettings(loadContentText(path), path.string(), path.parent_path());
    }

    int roomsForLevel(const RunSettings& run, int levelNumber)
    {
        if (levelNumber <= 0)
        {
            throw std::invalid_argument(
                std::format("Level {}: level numbers start at 1", levelNumber));
        }
        const std::int64_t rooms =
            run.firstRooms + (static_cast<std::int64_t>(levelNumber - 1) * run.roomsPerLevel);
        return static_cast<int>(std::min<std::int64_t>(rooms, run.maxRooms));
    }

    LevelGeneration levelGeneration(const RunSettings& run, int levelNumber, std::uint32_t seed)
    {
        return {.grid = run.grid, .roomCount = roomsForLevel(run, levelNumber), .seed = seed};
    }

    std::uint32_t runLevelSeed(std::uint32_t runSeed, int levelNumber)
    {
        LevelRandom random{
            (static_cast<std::uint64_t>(runSeed) << 32U) | static_cast<std::uint32_t>(levelNumber)};
        return static_cast<std::uint32_t>(nextRandom(random) >> 32U);
    }

    std::uint32_t nextRunSeed(std::uint32_t runSeed)
    {
        LevelRandom random{runSeed};
        return static_cast<std::uint32_t>(nextRandom(random) >> 32U);
    }
}
