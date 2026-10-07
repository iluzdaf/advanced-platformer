#pragma once

#include <cstdint>
#include <utility>

#include "content/game_catalogs.hpp"
#include "content/game_content.hpp"
#include "content/room_pieces.hpp"
#include "game/game.hpp"
#include "lua_npc_scripts.hpp"
#include "lua_presentation_script.hpp"
#include "support/atlas_size.hpp"
#include "support/fixed_step.hpp"

namespace tests
{
    constexpr const char* FixturePieces = "tests/fixtures/catalogs/pieces.json";

    inline advanced_platformer::GameContent fixtureContent(const char* pieces = FixturePieces)
    {
        advanced_platformer::GameContent content{
            advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", AtlasSize),
            advanced_platformer::LuaNpcScripts{},
            advanced_platformer::LuaPresentationScript{}};
        content.gameCatalogs.pieces = advanced_platformer::loadRoomPieceCatalog(pieces);
        return content;
    }

    inline advanced_platformer::Game fixtureGame(
        advanced_platformer::GameContent content,
        std::uint32_t runSeed = 1)
    {
        return {
            0,
            std::move(content.gameCatalogs),
            std::move(content.npcScripts),
            std::move(content.presentation),
            FixedStepSeconds,
            runSeed};
    }

    inline advanced_platformer::Game fixtureGame(const char* pieces = FixturePieces)
    {
        return fixtureGame(fixtureContent(pieces));
    }
}
