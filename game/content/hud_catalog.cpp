#include "hud_catalog.hpp"

#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "content_validation.hpp"

#include <filesystem>
#include <string_view>

#include <glaze/glaze.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/render/sprite.hpp"

namespace advanced_platformer
{
    struct HudIconJson
    {
        glm::vec2 position{};
        glm::vec2 size{};
    };

    struct HudJson
    {
        HudIconJson fullHeart;
        HudIconJson emptyHeart;
        HudIconJson bag;
    };

    namespace
    {
        void validateHudIcon(const SpriteRegion& region, std::string_view name)
        {
            if (!isFiniteNonNegative(region.position) || !isFinitePositive(region.size))
            {
                failJson(
                    {}, name, "expected a finite, non-negative atlas position and a positive size");
            }
        }

        SpriteRegion regionFrom(const HudIconJson& json)
        {
            return {json.position, json.size};
        }

        void validateHudIcons(const HudIcons& icons)
        {
            validateHudIcon(icons.fullHeart, "fullHeart");
            validateHudIcon(icons.emptyHeart, "emptyHeart");
            validateHudIcon(icons.bag, "bag");
        }
    }

    HudIcons parseHudIcons(std::string_view text, std::string_view sourceName)
    {
        const auto file = readContent<HudJson>(text, sourceName);
        HudIcons icons;
        icons.fullHeart = regionFrom(file.fullHeart);
        icons.emptyHeart = regionFrom(file.emptyHeart);
        icons.bag = regionFrom(file.bag);
        validateInFile(sourceName, [&] { validateHudIcons(icons); });
        return icons;
    }

    HudIcons loadHudIcons(const std::filesystem::path& path)
    {
        return parseHudIcons(loadContentText(path), path.string());
    }

    void validateHudAtlasRegions(
        const HudIcons& icons,
        glm::ivec2 atlasSize,
        std::string_view sourceName)
    {
        requireInAtlas(icons.fullHeart, atlasSize, sourceName, "fullHeart");
        requireInAtlas(icons.emptyHeart, atlasSize, sourceName, "emptyHeart");
        requireInAtlas(icons.bag, atlasSize, sourceName, "bag");
    }
}
