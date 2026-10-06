#pragma once

#include <utility>

#include "content/game_catalogs.hpp"
#include "content/game_content.hpp"
#include "content/run_settings.hpp"
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
            advanced_platformer::loadRunSettings("tests/fixtures/levels/actor_placement_run.json"),
            advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", AtlasSize),
            advanced_platformer::LuaNpcScripts{},
            advanced_platformer::LuaPresentationScript{}};
    }

    inline advanced_platformer::Game fixtureGame()
    {
        advanced_platformer::GameContent content = fixtureContent();
        return {
            0,
            std::move(content.run),
            std::move(content.gameCatalogs),
            std::move(content.npcScripts),
            std::move(content.presentation),
            FixedStepSeconds,
            1};
    }
}
