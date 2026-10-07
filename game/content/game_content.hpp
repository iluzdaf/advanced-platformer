#pragma once

#include <filesystem>

#include "game_catalogs.hpp"
#include "lua_npc_scripts.hpp"
#include "lua_presentation_script.hpp"

namespace advanced_platformer
{
    struct GameContent
    {
        GameCatalogs gameCatalogs;
        LuaNpcScripts npcScripts;
        LuaPresentationScript presentation;
    };

    GameContent loadGameContent(const std::filesystem::path& assetDirectory);
}
