#include <catch2/catch_test_macros.hpp>

#include "content/game_catalogs.hpp"
#include "content/level_catalog.hpp"
#include "game/game.hpp"
#include "lua_presentation_script.hpp"
#include "support/atlas_size.hpp"
#include "support/fixed_step.hpp"

TEST_CASE("Breaking a tile under a position breaks nothing that cannot break", "[app][debug]")
{
    advanced_platformer::Game game(
        0,
        advanced_platformer::loadLevelCatalog("tests/fixtures/levels/levels.json"),
        advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize),
        advanced_platformer::LuaNpcScripts{},
        advanced_platformer::LuaPresentationScript{},
        tests::FixedStepSeconds);
    REQUIRE_FALSE(game.breakTileAt({-100.0F, -100.0F}));
    REQUIRE_FALSE(game.breakTileAt({8.0F, 8.0F}));
}
