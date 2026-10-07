#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>

#include "game.hpp"
#include "level/level_composition.hpp"
#include "content/game_catalogs.hpp"
#include "level/level_generator.hpp"
#include "content/room_pieces.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/world/level_validation.hpp"
#include "support/fixed_step.hpp"
#include "support/fixture_game.hpp"

TEST_CASE("A run starts at level 1 with the seed its run seed gives", "[app][generation]")
{
    const advanced_platformer::Game game =
        tests::fixtureGame(tests::fixtureContent("tests/fixtures/rooms/finish/pieces.json"), 5);

    REQUIRE(game.runSeed() == 5U);
    REQUIRE(game.levelNumber() == 1);
    REQUIRE(game.levelSeed() == advanced_platformer::runLevelSeed(5, 1));
}

TEST_CASE(
    "Rerolling builds the level from the next seed and restarting keeps it",
    "[app][generation]")
{
    advanced_platformer::Game game =
        tests::fixtureGame(tests::fixtureContent("tests/fixtures/rooms/finish/pieces.json"), 5);
    const std::uint32_t seed = game.levelSeed();
    const advanced_platformer::Health health = game.playerHealth();

    game.changeLevel(advanced_platformer::LevelChange::Reroll);
    REQUIRE(game.levelNumber() == 1);
    REQUIRE(game.levelSeed() == seed + 1U);
    REQUIRE(game.playerHealth().current == health.current);

    game.changeLevel(advanced_platformer::LevelChange::Restart);
    REQUIRE(game.levelSeed() == seed + 1U);
    REQUIRE(game.runSeed() == 5U);
}

TEST_CASE("A generated level skips seeds whose exit the player cannot reach", "[app][generation]")
{
    const auto pieces = advanced_platformer::loadRoomPieceCatalog(
        "tests/fixtures/rooms/rooms_some_shut/pieces.json");
    const auto catalogs = advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs");
    bool skipped = false;
    for (std::uint32_t seed = 0; seed < 16; ++seed)
    {
        const advanced_platformer::GameLevel level = advanced_platformer::composePlayableLevel(
            pieces,
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
    const auto pieces = advanced_platformer::loadRoomPieceCatalog(
        "tests/fixtures/rooms/rooms_all_shut/pieces.json");
    const auto catalogs = advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs");

    REQUIRE_THROWS_WITH(
        advanced_platformer::composePlayableLevel(
            pieces,
            2,
            1,
            0,
            catalogs,
            advanced_platformer::composePlayer(catalogs, 0),
            tests::FixedStepSeconds),
        "Level 2: no seed from 1 to 100 gives a route from the spawn to the exit");
}
