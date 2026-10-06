#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/render/sprite.hpp"

#include "level_data.hpp"
#include "tile_catalog.hpp"

namespace advanced_platformer
{
    void validateContentSprite(const Sprite& sprite);
    void requireInAtlas(
        const SpriteRegion& region,
        glm::ivec2 atlasSize,
        std::string_view sourceName,
        std::string_view path);

    void validateExitSettings(
        const ExitPlacement& placement,
        const std::string& path = "exit",
        std::string_view sourceName = {});
    void validateTileCatalog(const TileCatalog& catalog);
    void validateTileLegend(const std::map<char, std::string>& legend, const TileCatalog& catalog);
    void validateLegendSymbols(
        const std::vector<std::string>& tileSymbols,
        std::string_view sourceName = {});
}
