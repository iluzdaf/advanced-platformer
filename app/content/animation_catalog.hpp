#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

#include <glm/vec2.hpp>

#include "advanced_platformer/render/animation.hpp"

namespace advanced_platformer
{
    using AnimationCatalog = std::map<std::string, AnimationSet>;
    void validateAnimationSet(const AnimationSet& set);
    void validateAnimationCatalog(const AnimationCatalog& catalog);
    AnimationCatalog parseAnimationCatalog(std::string_view text, std::string_view sourceName);
    AnimationCatalog loadAnimationCatalog(const std::filesystem::path& path);
    void validateAnimationAtlasRegions(
        const AnimationCatalog& catalog,
        glm::ivec2 atlasSize,
        std::string_view sourceName);
    const AnimationSet& animationSet(const AnimationCatalog& catalog, const std::string& name);
    const char* clipName(AnimationName name);
}
