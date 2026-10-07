#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

#include <glm/vec2.hpp>

#include "advanced_platformer/render/sprite.hpp"
#include "advanced_platformer/world/level_exit.hpp"

namespace advanced_platformer
{
    struct ExitDefinition
    {
        glm::vec2 bodySize = {0.0F, 0.0F};
        Sprite sprite;
    };

    using ExitCatalog = std::map<std::string, ExitDefinition>;
    void validateExitDefinition(const ExitDefinition& definition);
    void validateExitCatalog(const ExitCatalog& catalog);
    ExitCatalog parseExitCatalog(std::string_view text, std::string_view sourceName);
    ExitCatalog loadExitCatalog(const std::filesystem::path& path);
    void validateExitAtlasRegions(
        const ExitCatalog& catalog,
        glm::ivec2 atlasSize,
        std::string_view sourceName);
    const ExitDefinition& exitDefinition(const ExitCatalog& catalog, const std::string& name);
    LevelExit composeExit(const ExitDefinition& definition, int textureId, glm::vec2 spawnFeet);
}
