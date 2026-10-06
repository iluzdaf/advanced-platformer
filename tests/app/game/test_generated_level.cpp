#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>

#include "game/game.hpp"
#include "game/level_composition.hpp"
#include "lua_presentation_script.hpp"
#include "content/game_catalogs.hpp"
#include "content/run_settings.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/world/level_validation.hpp"
#include "support/atlas_size.hpp"
#include "support/fixed_step.hpp"

namespace
{
    advanced_platformer::Game gameFrom(const char* run, std::uint32_t runSeed)
    {
        return {
            0,
            advanced_platformer::loadRunSettings(run),
            advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize),
            advanced_platformer::LuaNpcScripts{},
            advanced_platformer::LuaPresentationScript{},
            tests::FixedStepSeconds,
            runSeed};
    }
}

TEST_CASE("A run starts at level 1 with the seed its run seed gives", "[app][generation]")
{
    const advanced_platformer::Game game = gameFrom("tests/fixtures/levels/finish_run.json", 5);

    REQUIRE(game.runSeed() == 5U);
    REQUIRE(game.levelNumber() == 1);
    REQUIRE(game.levelSeed() == advanced_platformer::runLevelSeed(5, 1));
}

TEST_CASE(
    "Rerolling builds the level from the next seed and restarting keeps it",
    "[app][generation]")
{
    advanced_platformer::Game game = gameFrom("tests/fixtures/levels/finish_run.json", 5);
    const std::uint32_t seed = game.levelSeed();
    const advanced_platformer::Health health = game.playerHealth();

    game.rerollLevel();
    REQUIRE(game.levelNumber() == 1);
    REQUIRE(game.levelSeed() == seed + 1U);
    REQUIRE(game.playerHealth().current == health.current);

    game.restartLevel();
    REQUIRE(game.levelSeed() == seed + 1U);
    REQUIRE(game.runSeed() == 5U);
}

TEST_CASE("A generated level skips seeds whose exit the player cannot reach", "[app][generation]")
{
    const auto run =
        advanced_platformer::loadRunSettings("tests/fixtures/levels/rooms_some_shut_run.json");
    const auto catalogs =
        advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize);
    bool skipped = false;
    for (std::uint32_t seed = 0; seed < 16; ++seed)
    {
        const advanced_platformer::GameLevel level = advanced_platformer::composeStartedLevel(
            run,
            1,
            seed,
            0,
            catalogs,
            advanced_platformer::composePlayer(catalogs, 0),
            tests::FixedStepSeconds);
        INFO("Seed " << seed);
        REQUIRE(level.seed >= seed);
        REQUIRE(
            advanced_platformer::playerCanReachExit(
                level.map, level.world, tests::FixedStepSeconds));
        skipped = skipped || level.seed != seed;
    }
    REQUIRE(skipped);
}

TEST_CASE("A generated level whose every seed is shut off fails to start", "[app][generation]")
{
    const auto run =
        advanced_platformer::loadRunSettings("tests/fixtures/levels/rooms_all_shut_run.json");
    const auto catalogs =
        advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize);

    REQUIRE_THROWS_WITH(
        advanced_platformer::composeStartedLevel(
            run,
            2,
            1,
            0,
            catalogs,
            advanced_platformer::composePlayer(catalogs, 0),
            tests::FixedStepSeconds),
        "Level 2: no seed from 1 to 100 gives a route from the spawn to the exit");
}
