#include "content_validation.hpp"

#include "content_diagnostics.hpp"
#include "tile_catalog.hpp"

#include <cstddef>
#include <format>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/render/sprite.hpp"
#include "advanced_platformer/world/tile_map.hpp"

namespace advanced_platformer
{
    void validateContentSprite(const Sprite& sprite)
    {
        if (!isFiniteNonNegative(sprite.region.position) || !isFinitePositive(sprite.region.size))
        {
            throw std::invalid_argument(
                "sprite requires finite non-negative atlas position and positive size");
        }
    }

    void requireInAtlas(
        const SpriteRegion& region,
        glm::ivec2 atlasSize,
        std::string_view sourceName,
        std::string_view path)
    {
        const glm::vec2 far = region.position + region.size;
        if (far.x > static_cast<float>(atlasSize.x) || far.y > static_cast<float>(atlasSize.y))
        {
            failJson(
                sourceName,
                path,
                std::format("region runs past the {} by {} atlas", atlasSize.x, atlasSize.y));
        }
    }

    void validateTileCatalog(const TileCatalog& catalog)
    {
        if (catalog.tileSize <= 0)
        {
            throw std::invalid_argument("tile catalog needs a positive tileSize");
        }
        const auto empty = catalog.ids.find("empty");
        if (empty == catalog.ids.end() || empty->second != 0 || catalog.definitions.empty())
        {
            throw std::invalid_argument("tile catalog must reserve ID zero for 'empty'");
        }
        if (catalog.definitions.front().blocksMovement || catalog.definitions.front().blocksSight ||
            catalog.definitions.front().climbable)
        {
            throw std::invalid_argument("empty must allow movement and sight and not be climbable");
        }
        std::set<int> usedIds;
        for (const auto& entry : catalog.ids)
        {
            const int id = entry.second;
            if (id < 0 || static_cast<std::size_t>(id) >= catalog.definitions.size() ||
                !usedIds.insert(id).second)
            {
                throw std::invalid_argument(
                    std::format("invalid or repeated tile ID for '{}'", entry.first));
            }
            if (id == 0)
            {
                continue;
            }
            const TileDefinition& definition = catalog.definitions[static_cast<std::size_t>(id)];
            if (definition.climbable && !definition.blocksMovement)
            {
                throw std::invalid_argument(
                    std::format("climbable tile '{}' must block movement", entry.first));
            }
            const auto& sprite = definition.sprite;
            if (!isFiniteNonNegative(sprite.position) || !isFinitePositive(sprite.size))
            {
                throw std::invalid_argument(
                    std::format("invalid sprite region for tile '{}'", entry.first));
            }
            if (sprite.size != glm::vec2{catalog.tileSize, catalog.tileSize})
            {
                throw std::invalid_argument(
                    std::format(
                        "tile '{}' sprite must be {} by {} atlas pixels, the catalog's tileSize",
                        entry.first,
                        catalog.tileSize,
                        catalog.tileSize));
            }
            const auto& breaksInto =
                catalog.definitions[static_cast<std::size_t>(id)].breaksIntoTileId;
            if (breaksInto.has_value() &&
                (*breaksInto < 0 ||
                 static_cast<std::size_t>(*breaksInto) >= catalog.definitions.size() ||
                 *breaksInto == id))
            {
                throw std::invalid_argument(
                    std::format("invalid breaksInto tile for '{}'", entry.first));
            }
        }
        if (usedIds.size() != catalog.definitions.size())
        {
            throw std::invalid_argument("every tile definition must have a catalog name");
        }
    }

    void validateTileLegend(const std::map<char, std::string>& legend, const TileCatalog& catalog)
    {
        if (legend.empty())
        {
            throw std::invalid_argument("tileLegend: expected at least one symbol");
        }
        for (const auto& entry : legend)
        {
            if (catalog.ids.find(entry.second) == catalog.ids.end())
            {
                throw std::invalid_argument(
                    std::format("Unknown tile name '{}' in tileLegend", entry.second));
            }
        }
    }

    void validateLegendSymbols(
        const std::vector<std::string>& tileSymbols,
        std::string_view sourceName)
    {
        std::set<std::string> tiles;
        for (const auto& symbol : tileSymbols)
        {
            if (symbol.size() != 1)
            {
                failJson(sourceName, "tileLegend", "symbols must be one character");
            }
            if (!tiles.insert(symbol).second)
            {
                failJson(sourceName, fieldPath("tileLegend", symbol), "repeated symbol");
            }
        }
    }
}
