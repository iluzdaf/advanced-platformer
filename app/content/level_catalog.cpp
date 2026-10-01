#include "level_catalog.hpp"
#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "content_json.hpp"
#include <format>

#include <algorithm>
#include <cstddef>
#include <filesystem>
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
    // levels.json as written: its member names are the file's keys. Glaze reflects only types
    // with linkage, so these cannot go in an anonymous namespace.
    struct LevelEntryJson
    {
        int number = 0;
        std::string file;
    };

    struct LevelCatalogJson
    {
        int startLevel = 0;
        glm::vec2 cameraDeadZone{};
        std::vector<LevelEntryJson> levels;
    };

    namespace
    {
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
                failJson(sourceName, path, "file must be a non-empty relative path");
            }
            for (const std::filesystem::path& part : file)
            {
                if (part == "..")
                {
                    failJson(sourceName, path, "file must stay inside the level directory");
                }
            }
            return file;
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
            entry.relativeFile = relativeFileFrom(json.file, sourceName, fieldPath(path, "file"));

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

    std::filesystem::path levelPath(const LevelCatalog& catalog, int levelNumber)
    {
        const auto found = std::ranges::find_if(
            catalog.levels,
            [levelNumber](const LevelCatalogEntry& entry) { return entry.number == levelNumber; });
        if (found == catalog.levels.end())
        {
            throw std::invalid_argument(std::format("Level {} is not in the catalog", levelNumber));
        }
        return catalog.levelDirectory / found->relativeFile;
    }
}
