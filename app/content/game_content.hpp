#pragma once

#include <filesystem>

#include <glm/vec2.hpp>

#include "game_catalogs.hpp"
#include "level_catalog.hpp"
#include "lua_npc_scripts.hpp"
#include "lua_presentation_script.hpp"

namespace advanced_platformer
{
    struct GameContent
    {
        LevelCatalog levelCatalog;
        GameCatalogs gameCatalogs;
        LuaNpcScripts npcScripts;
        LuaPresentationScript presentation;
    };

    GameContent loadGameContent(const std::filesystem::path& assetDirectory, glm::ivec2 atlasSize);
}
