#pragma once
#include <filesystem>
#include <string_view>

#include <glm/vec2.hpp>

#include "advanced_platformer/render/sprite.hpp"

namespace advanced_platformer
{
    struct HudIcons
    {
        SpriteRegion fullHeart;
        SpriteRegion emptyHeart;
        SpriteRegion bag;
    };

    HudIcons parseHudIcons(std::string_view text, std::string_view sourceName);
    HudIcons loadHudIcons(const std::filesystem::path& path);
    void validateHudAtlasRegions(
        const HudIcons& icons,
        glm::ivec2 atlasSize,
        std::string_view sourceName);
}
