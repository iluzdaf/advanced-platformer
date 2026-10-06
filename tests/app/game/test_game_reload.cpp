#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>
#include <format>
#include <stdexcept>
#include <utility>

#include "content/game_catalogs.hpp"
#include "content/game_content.hpp"
#include "content/run_settings.hpp"
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
            advanced_platformer::loadRunSettings("tests/fixtures/levels/actor_placement_run.json"),
            advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize),
            advanced_platformer::LuaNpcScripts{},
            advanced_platformer::LuaPresentationScript{}};
    }

    advanced_platformer::Game game()
    {
        advanced_platformer::GameContent loaded = content();
        return {
            0,
            std::move(loaded.run),
            std::move(loaded.gameCatalogs),
            std::move(loaded.npcScripts),
            std::move(loaded.presentation),
            tests::FixedStepSeconds,
            1};
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

TEST_CASE("A reload that shuts off the exit leaves the game as it was", "[app][reload]")
{
    advanced_platformer::Game running = {
        0,
        advanced_platformer::loadRunSettings("tests/fixtures/levels/rooms_some_shut_run.json"),
        advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize),
        advanced_platformer::LuaNpcScripts{},
        advanced_platformer::LuaPresentationScript{},
        tests::FixedStepSeconds,
        1};
    const std::uint32_t seed = running.levelSeed();
    advanced_platformer::GameContent shut = content();
    shut.run =
        advanced_platformer::loadRunSettings("tests/fixtures/levels/rooms_all_shut_run.json");

    REQUIRE_THROWS_WITH(
        running.reload(std::move(shut)),
        std::format("Level 1: seed {} has no route from the spawn to the exit", seed));

    REQUIRE(running.levelSeed() == seed);
    REQUIRE(running.levelNumber() == 1);
}
