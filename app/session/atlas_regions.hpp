#pragma once

#include <filesystem>

#include <glm/vec2.hpp>

namespace advanced_platformer
{
    struct GameCatalogs;

    void validateAtlasRegions(
        const GameCatalogs& catalogs,
        glm::ivec2 atlasSize,
        const std::filesystem::path& catalogDirectory);
}
