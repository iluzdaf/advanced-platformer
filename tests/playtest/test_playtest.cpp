#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>
#include <vector>

#include "game.hpp"
#include "playtest.hpp"
#include "support/fixed_step.hpp"
#include "support/fixture_game.hpp"

TEST_CASE("A playtest bot follows the route to the exit")
{
    advanced_platformer::Game game = tests::fixtureGame(
        advanced_platformer::playtestContent(
            tests::fixtureContent(), ADVANCED_PLATFORMER_SOURCE_PLAYTEST_ASSETS));

    const std::vector<advanced_platformer::LevelPlaytest> results =
        advanced_platformer::playtestRun(game, 1, 60.0F, tests::FixedStepSeconds);

    REQUIRE(results.size() == 1);
    CHECK(results.front().outcome == advanced_platformer::PlaytestOutcome::Exit);
    CHECK(results.front().level == 1);
    CHECK(game.levelNumber() == 2);
}

TEST_CASE("A playtest samples the level's pacing every half second")
{
    advanced_platformer::Game game = tests::fixtureGame(
        advanced_platformer::playtestContent(
            tests::fixtureContent(), ADVANCED_PLATFORMER_SOURCE_PLAYTEST_ASSETS));

    const std::vector<advanced_platformer::LevelPlaytest> results =
        advanced_platformer::playtestRun(game, 1, 60.0F, tests::FixedStepSeconds);

    REQUIRE(results.size() == 1);
    const std::vector<advanced_platformer::PacingSample>& pacing = results.front().pacing;
    REQUIRE(pacing.size() > 2);
    CHECK(pacing.front().seconds == 0.0F);
    CHECK(pacing.front().health > 0);
    CHECK(pacing.back().seconds == Catch::Approx(results.front().seconds).margin(0.001));
    for (std::size_t sample = 1; sample < pacing.size(); ++sample)
    {
        INFO("sample " << sample);
        CHECK(pacing[sample].seconds > pacing[sample - 1].seconds);
        CHECK(pacing[sample].seconds - pacing[sample - 1].seconds <= Catch::Approx(0.5F));
        CHECK_FALSE(pacing[sample].piece.empty());
    }
}

TEST_CASE("A playtest moves on to the next level after a timeout")
{
    advanced_platformer::Game game = tests::fixtureGame(
        advanced_platformer::playtestContent(
            tests::fixtureContent(), ADVANCED_PLATFORMER_SOURCE_PLAYTEST_ASSETS));

    const std::vector<advanced_platformer::LevelPlaytest> results =
        advanced_platformer::playtestRun(game, 2, tests::FixedStepSeconds, tests::FixedStepSeconds);

    REQUIRE(results.size() == 2);
    CHECK(results[0].outcome == advanced_platformer::PlaytestOutcome::Timeout);
    CHECK(results[0].level == 1);
    CHECK(results[1].level == 2);
    CHECK(results[1].runSeed == results[0].runSeed);
}

TEST_CASE("A level playtest is written as one JSON line")
{
    const advanced_platformer::LevelPlaytest level{
        .runSeed = 7,
        .level = 2,
        .levelSeed = 11,
        .outcome = advanced_platformer::PlaytestOutcome::Stuck,
        .damageByNearestNpc = {{"rat", 2}},
        .endCell = {3, 4},
        .pacing = {{.seconds = 0.5F, .health = 3, .damage = 1, .npcsNear = 2, .piece = "den"}}};

    const std::string text = advanced_platformer::formatLevelPlaytest(level);

    CHECK(text.find('\n') == std::string::npos);
    CHECK(text.find(R"("outcome":"stuck")") != std::string::npos);
    CHECK(text.find(R"("damageByNearestNpc":{"rat":2})") != std::string::npos);
    CHECK(text.find(R"("endCell":[3,4])") != std::string::npos);
    CHECK(
        text.find(
            R"("pacing":[{"seconds":0.5,"health":3,"damage":1,"npcsNear":2,"piece":"den"}])") !=
        std::string::npos);
}
