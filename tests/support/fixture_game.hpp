#pragma once

#include <utility>

#include "content/game_catalogs.hpp"
#include "content/game_content.hpp"
#include "content/level_catalog.hpp"
#include "game/game.hpp"
#include "lua_npc_scripts.hpp"
#include "lua_presentation_script.hpp"
#include "support/atlas_size.hpp"
#include "support/fixed_step.hpp"

namespace tests
{
    inline advanced_platformer::GameContent fixtureContent()
    {
        return {
            advanced_platformer::loadLevelCatalog(
                "tests/fixtures/levels/actor_placement_levels.json"),
            advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", AtlasSize),
            advanced_platformer::LuaNpcScripts{},
            advanced_platformer::LuaPresentationScript{}};
    }

    inline advanced_platformer::Game fixtureGame()
    {
        advanced_platformer::GameContent content = fixtureContent();
        return {
            0,
            std::move(content.levelCatalog),
            std::move(content.gameCatalogs),
            std::move(content.npcScripts),
            std::move(content.presentation),
            FixedStepSeconds};
    }
}
