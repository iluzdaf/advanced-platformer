#include "tile_catalog.hpp"

#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "content_validation.hpp"

#include <cstddef>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glaze/glaze.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/world/tile_map.hpp"

namespace advanced_platformer
{
    struct TileSpriteJson
    {
        glm::vec2 position{};
    };

    struct TileJson
    {
        bool blocksMovement = false;
        bool blocksSight = false;
        std::optional<bool> climbable;
        std::optional<TileSpriteJson> sprite;
        std::optional<std::string> breaksInto;
        std::optional<WithDefaults<TouchDamage>> touchDamage;
    };

    struct TilesJson
    {
        int tileSize = 0;
        std::map<std::string, TileJson> tiles;
    };

    TileCatalog parseTileCatalog(std::string_view text, std::string_view sourceName)
    {
        const auto file = readContent<TilesJson>(text, sourceName);
        if (!file.tiles.contains("empty"))
        {
            failJson(sourceName, "tiles", "missing 'empty'");
        }
        TileCatalog result;
        result.tileSize = file.tileSize;
        std::map<std::string, std::string> breaksIntoNames;
        const auto add =
            [&result, &breaksIntoNames, sourceName](const std::string& name, const TileJson& json)
        {
            const std::string path = fieldPath("tiles", name);
            TileDefinition definition;
            definition.blocksMovement = json.blocksMovement;
            definition.blocksSight = json.blocksSight;
            definition.climbable = json.climbable.value_or(false);
            if (json.touchDamage.has_value())
            {
                definition.touchDamage = json.touchDamage->get();
            }
            if (name == "empty")
            {
                if (json.sprite.has_value())
                {
                    failJson(sourceName, path, "unknown field 'sprite'");
                }
                if (json.breaksInto.has_value())
                {
                    failJson(sourceName, path, "unknown field 'breaksInto'");
                }
            }
            else
            {
                if (!json.sprite.has_value())
                {
                    failJson(sourceName, path, "missing 'sprite'");
                }
                const auto side = static_cast<float>(result.tileSize);
                definition.sprite = {json.sprite->position, {side, side}};
                if (json.breaksInto.has_value())
                {
                    breaksIntoNames.emplace(name, *json.breaksInto);
                }
            }
            result.ids.emplace(name, static_cast<int>(result.definitions.size()));
            result.definitions.push_back(definition);
        };
        add("empty", file.tiles.at("empty"));
        for (const auto& [name, json] : file.tiles)
        {
            if (name != "empty")
            {
                add(name, json);
            }
        }
        for (const auto& entry : breaksIntoNames)
        {
            const std::string path = fieldPath(fieldPath("tiles", entry.first), "breaksInto");
            const auto target = result.ids.find(entry.second);
            if (target == result.ids.end())
            {
                failJson(sourceName, path, std::format("unknown tile name '{}'", entry.second));
            }
            const auto broken = static_cast<std::size_t>(result.ids.at(entry.first));
            result.definitions[broken].breaksIntoTileId = target->second;
        }
        validateInFile(sourceName, [&] { validateTileCatalog(result); });
        return result;
    }

    TileCatalog loadTileCatalog(const std::filesystem::path& path)
    {
        return parseTileCatalog(loadContentText(path), path.string());
    }

    void validateTileAtlasRegions(
        const TileCatalog& catalog,
        glm::ivec2 atlasSize,
        std::string_view sourceName)
    {
        for (const auto& [name, id] : catalog.ids)
        {
            if (id != 0)
            {
                requireInAtlas(
                    catalog.definitions[static_cast<std::size_t>(id)].sprite,
                    atlasSize,
                    sourceName,
                    fieldPath(fieldPath("tiles", name), "sprite"));
            }
        }
    }

    TileMap composeTileMap(
        const std::vector<std::string>& rows,
        const std::map<char, std::string>& legend,
        const TileCatalog& catalog)
    {
        validateTileCatalog(catalog);
        validateTileLegend(legend, catalog);
        std::map<char, int> ids;
        for (const auto& entry : legend)
        {
            ids.emplace(entry.first, catalog.ids.at(entry.second));
        }
        return TileMap::fromAscii(catalog.tileSize, rows, catalog.definitions, ids);
    }
}
