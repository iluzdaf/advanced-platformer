#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <format>
#include <sstream>

#include "debug/console_log.hpp"
#include "game/game.hpp"
#include "game/level_requests.hpp"
#include "game/play_control.hpp"
#include "advanced_platformer/timing/fixed_step.hpp"
#include "advanced_platformer/timing/frame_profile.hpp"
#include "support/fixed_step.hpp"
#include "support/fixture_game.hpp"

namespace
{
    bool playInterrupted(advanced_platformer::PlayControl& play, advanced_platformer::Game& game)
    {
        advanced_platformer::FixedStep fixedStep;
        advanced_platformer::FrameProfile profile;
        profile.frameSeconds = 2.0F * tests::FixedStepSeconds;
        play.advance(game, fixedStep, {}, profile);
        return profile.simulationTicks == 0;
    }
}

TEST_CASE("A level restart keeps the seed and interrupts play", "[app][level-requests]")
{
    advanced_platformer::Game game = tests::fixtureGame();
    advanced_platformer::PlayControl play;
    std::ostringstream echo;
    advanced_platformer::ConsoleLog console(echo);
    const std::uint32_t seed = game.levelSeed();
    advanced_platformer::LevelRequests requests;
    requests.restartLevel = true;

    applyLevelRequests(requests, game, play, console);

    REQUIRE(game.levelSeed() == seed);
    REQUIRE(game.levelNumber() == 1);
    REQUIRE_FALSE(requests.restartLevel);
    REQUIRE(console.written() == 0);
    REQUIRE(playInterrupted(play, game));
}

TEST_CASE("A reroll generates the next seed and reports it", "[app][level-requests]")
{
    advanced_platformer::Game game = tests::fixtureGame();
    advanced_platformer::PlayControl play;
    std::ostringstream echo;
    advanced_platformer::ConsoleLog console(echo);
    const std::uint32_t seed = game.levelSeed();
    advanced_platformer::LevelRequests requests;
    requests.rerollLevel = true;

    applyLevelRequests(requests, game, play, console);

    REQUIRE(game.levelSeed() == seed + 1U);
    REQUIRE(console.entries().size() == 1);
    REQUIRE(console.entries().back().level == advanced_platformer::ConsoleLevel::Info);
    REQUIRE(
        console.entries().back().text == std::format("Generated level 1 from seed {}", seed + 1U));
    REQUIRE_FALSE(requests.rerollLevel);
    REQUIRE(playInterrupted(play, game));
}

TEST_CASE("A game restart leaves a running game alone", "[app][level-requests]")
{
    advanced_platformer::Game game = tests::fixtureGame();
    advanced_platformer::PlayControl play;
    std::ostringstream echo;
    advanced_platformer::ConsoleLog console(echo);
    const std::uint32_t seed = game.levelSeed();
    play.toggleInventory();
    advanced_platformer::LevelRequests requests;
    requests.restartGame = true;

    applyLevelRequests(requests, game, play, console);

    REQUIRE(game.levelSeed() == seed);
    REQUIRE(play.inventoryOpen());
    REQUIRE_FALSE(requests.restartGame);
    REQUIRE(console.written() == 0);
}

TEST_CASE("An applied request does not repeat on the next frame", "[app][level-requests]")
{
    advanced_platformer::Game game = tests::fixtureGame();
    advanced_platformer::PlayControl play;
    std::ostringstream echo;
    advanced_platformer::ConsoleLog console(echo);
    const std::uint32_t seed = game.levelSeed();
    advanced_platformer::LevelRequests requests;
    requests.rerollLevel = true;

    applyLevelRequests(requests, game, play, console);
    applyLevelRequests(requests, game, play, console);

    REQUIRE(game.levelSeed() == seed + 1U);
    REQUIRE(console.written() == 1);
}
