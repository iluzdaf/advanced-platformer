#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <utility>

#include "content/game_catalogs.hpp"
#include "content/game_content.hpp"
#include "content/level_catalog.hpp"
#include "game/game.hpp"
#include "game/level_reload.hpp"
#include "lua_npc_scripts.hpp"
#include "lua_presentation_script.hpp"
#include "support/atlas_size.hpp"
#include "support/fixed_step.hpp"

namespace
{
    advanced_platformer::GameContent content()
    {
        return {
            advanced_platformer::parseLevelCatalog(
                R"({"startLevel":1,"cameraDeadZone":[80,45],"levels":[{"number":1,"file":"actor_placement.json"}]})",
                "fixture",
                "tests/fixtures/levels"),
            advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize),
            advanced_platformer::LuaNpcScripts{},
            advanced_platformer::LuaPresentationScript{}};
    }

    advanced_platformer::Game game()
    {
        advanced_platformer::GameContent loaded = content();
        return {
            0,
            std::move(loaded.levelCatalog),
            std::move(loaded.gameCatalogs),
            std::move(loaded.npcScripts),
            std::move(loaded.presentation),
            tests::FixedStepSeconds};
    }
}

TEST_CASE("A reload applies new definitions to the running game", "[app][reload]")
{
    advanced_platformer::Game running = game();
    advanced_platformer::GameContent changed = content();
    changed.gameCatalogs.actors.definitions.at("test_player").health = 5;

    const advanced_platformer::LevelReload reload = running.reload(std::move(changed));

    REQUIRE(running.playerHealth().maximum == 5);
    REQUIRE(running.playerHealth().current == 3);
    REQUIRE(reload.kept == 1);
    REQUIRE(reload.spawned.empty());
    REQUIRE(reload.removed.empty());
}

TEST_CASE("A failed reload leaves the game as it was", "[app][reload]")
{
    advanced_platformer::Game running = game();
    advanced_platformer::GameContent broken = content();
    broken.gameCatalogs.actors.definitions.erase("test_guard");
    broken.gameCatalogs.actors.definitions.at("test_player").health = 5;

    REQUIRE_THROWS_AS(running.reload(std::move(broken)), std::invalid_argument);

    REQUIRE(running.playerHealth().maximum == 3);
    running.update({}, tests::FixedStepSeconds);
    REQUIRE(running.reload(content()).kept == 1);
}
