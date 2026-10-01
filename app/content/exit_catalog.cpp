#include "exit_catalog.hpp"

#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "content_validation.hpp"

#include <filesystem>
#include <format>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>

#include <glaze/glaze.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/render/sprite.hpp"
#include "advanced_platformer/world/level_exit.hpp"

namespace advanced_platformer
{
    // exits.json as written: its member names are the file's keys. Glaze reflects only types
    // with linkage, so these cannot go in an anonymous namespace.
    struct ExitJson
    {
        glm::vec2 bodySize{};
        SpriteJson sprite;
    };

    struct ExitsJson
    {
        std::map<std::string, ExitJson> exits;
    };

    void validateExitDefinition(const ExitDefinition& definition)
    {
        LevelExit exit;
        exit.bounds.size = definition.bodySize;
        validateLevelExit(exit);
        validateContentSprite(definition.sprite);
    }

    void validateExitCatalog(const ExitCatalog& catalog)
    {
        for (const auto& entry : catalog)
        {
            try
            {
                if (entry.first.empty())
                {
                    throw std::invalid_argument("exit definition name cannot be empty");
                }
                validateExitDefinition(entry.second);
            }
            catch (const std::invalid_argument& error)
            {
                failJson({}, fieldPath("exits", entry.first), error.what());
            }
        }
    }

    ExitCatalog parseExitCatalog(std::string_view text, std::string_view sourceName)
    {
        const auto file = readContent<ExitsJson>(text, sourceName);
        ExitCatalog catalog;
        for (const auto& [name, json] : file.exits)
        {
            catalog.emplace(name, ExitDefinition{json.bodySize, spriteFrom(json.sprite)});
        }
        validateInFile(sourceName, [&] { validateExitCatalog(catalog); });
        return catalog;
    }

    ExitCatalog loadExitCatalog(const std::filesystem::path& path)
    {
        return parseExitCatalog(loadContentText(path), path.string());
    }

    void validateExitAtlasRegions(
        const ExitCatalog& catalog,
        glm::ivec2 atlasSize,
        std::string_view sourceName)
    {
        for (const auto& [name, definition] : catalog)
        {
            requireInAtlas(
                definition.sprite.region,
                atlasSize,
                sourceName,
                fieldPath(fieldPath("exits", name), "sprite"));
        }
    }

    const ExitDefinition& exitDefinition(const ExitCatalog& catalog, const std::string& name)
    {
        const auto found = catalog.find(name);
        if (found == catalog.end())
        {
            throw std::invalid_argument(std::format("unknown exit definition '{}'", name));
        }
        return found->second;
    }

    LevelExit composeExit(const ExitDefinition& definition, int textureId, glm::vec2 spawnFeet)
    {
        validateExitDefinition(definition);
        LevelExit exit;
        exit.bounds = boxStandingOn(spawnFeet, definition.bodySize);
        Sprite sprite = definition.sprite;
        sprite.textureId = textureId;
        exit.sprite = sprite;
        validateLevelExit(exit);
        return exit;
    }
}
