#include "level_catalog.hpp"

#include "content_diagnostics.hpp"
#include "content_glaze.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glaze/glaze.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/math/validation.hpp"

namespace advanced_platformer
{
    struct LevelEntryJson
    {
        int number = 0;
        std::string pieces;
        int rooms = 0;
        std::optional<std::array<int, 2>> grid;
        std::optional<std::uint32_t> seed;
        std::optional<int> nextLevel;
    };

    struct LevelCatalogJson
    {
        int startLevel = 0;
        glm::vec2 cameraDeadZone{};
        std::vector<LevelEntryJson> levels;
    };

    namespace
    {
        constexpr std::array<int, 2> DefaultGenerationGrid = {9, 7};

        void requirePositiveLevel(int number, std::string_view sourceName, std::string_view path)
        {
            if (number <= 0)
            {
                failJson(sourceName, path, "level number must be positive");
            }
        }

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

        LevelGeneration generationFrom(
            const LevelEntryJson& json,
            std::string_view sourceName,
            std::string_view path)
        {
            LevelGeneration result;
            result.relativePieces =
                relativeFileFrom(json.pieces, sourceName, fieldPath(path, "pieces"));
            const std::array<int, 2> grid = json.grid.value_or(DefaultGenerationGrid);
            result.grid = {grid[0], grid[1]};
            if (result.grid.width <= 0 || result.grid.height <= 0)
            {
                failJson(sourceName, fieldPath(path, "grid"), "expected a positive size");
            }
            result.roomCount = json.rooms;
            if (result.roomCount < 2 || result.roomCount > result.grid.width * result.grid.height)
            {
                failJson(
                    sourceName,
                    fieldPath(path, "rooms"),
                    std::format(
                        "expected at least 2 rooms and no more than the grid's {} slots",
                        result.grid.width * result.grid.height));
            }
            result.seed = json.seed.value_or(static_cast<std::uint32_t>(json.number));
            if (json.nextLevel.has_value())
            {
                requirePositiveLevel(*json.nextLevel, sourceName, fieldPath(path, "nextLevel"));
            }
            result.nextLevel = json.nextLevel;
            return result;
        }
    }

    LevelCatalog parseLevelCatalog(
        std::string_view text,
        std::string_view sourceName,
        const std::filesystem::path& levelDirectory)
    {
        const auto file = readContent<LevelCatalogJson>(text, sourceName);
        LevelCatalog result;
        requirePositiveLevel(file.startLevel, sourceName, "startLevel");
        result.startLevel = file.startLevel;
        result.cameraDeadZone = file.cameraDeadZone;
        if (!isFinitePositive(result.cameraDeadZone) ||
            result.cameraDeadZone.x > InternalViewportSize.x ||
            result.cameraDeadZone.y > InternalViewportSize.y)
        {
            failJson(
                sourceName,
                "cameraDeadZone",
                std::format(
                    "expected a positive size that fits in the {} by {} view",
                    InternalWidth,
                    InternalHeight));
        }
        result.levelDirectory = levelDirectory;

        if (file.levels.empty())
        {
            failJson(sourceName, "levels", "expected at least one level");
        }
        result.levels.reserve(file.levels.size());
        for (std::size_t index = 0; index < file.levels.size(); ++index)
        {
            const LevelEntryJson& json = file.levels[index];
            const std::string path = indexPath("levels", index);
            requirePositiveLevel(json.number, sourceName, fieldPath(path, "number"));
            LevelCatalogEntry entry;
            entry.number = json.number;
            entry.generation = generationFrom(json, sourceName, path);

            const auto duplicateNumber = std::ranges::find_if(
                result.levels,
                [&entry](const LevelCatalogEntry& existing)
                { return existing.number == entry.number; });
            if (duplicateNumber != result.levels.end())
            {
                failJson(sourceName, fieldPath(path, "number"), "level number is already listed");
            }
            result.levels.push_back(std::move(entry));
        }

        const auto start = std::ranges::find_if(
            result.levels,
            [&result](const LevelCatalogEntry& entry)
            { return entry.number == result.startLevel; });
        if (start == result.levels.end())
        {
            failJson(sourceName, "startLevel", "level is not listed in the catalog");
        }
        return result;
    }

    LevelCatalog loadLevelCatalog(const std::filesystem::path& path)
    {
        return parseLevelCatalog(loadContentText(path), path.string(), path.parent_path());
    }

    const LevelCatalogEntry& levelEntry(const LevelCatalog& catalog, int levelNumber)
    {
        const auto found = std::ranges::find_if(
            catalog.levels,
            [levelNumber](const LevelCatalogEntry& entry) { return entry.number == levelNumber; });
        if (found == catalog.levels.end())
        {
            throw std::invalid_argument(std::format("Level {} is not in the catalog", levelNumber));
        }
        return *found;
    }
}
