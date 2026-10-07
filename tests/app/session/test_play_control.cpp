#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "game.hpp"
#include "session/play_control.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/timing/fixed_step.hpp"
#include "advanced_platformer/timing/frame_profile.hpp"
#include "support/fixed_step.hpp"
#include "support/fixture_game.hpp"

namespace
{
    constexpr float FrameSeconds = 3.5F * tests::FixedStepSeconds;
    constexpr int TicksPerFrame = 3;
    constexpr glm::vec2 Cursor = {0.0F, 0.0F};

    int advanceFrame(
        advanced_platformer::PlayControl& play,
        advanced_platformer::Game& game,
        advanced_platformer::FixedStep& fixedStep,
        const advanced_platformer::PlayFrame& frame = {})
    {
        advanced_platformer::FrameProfile profile;
        profile.frameSeconds = FrameSeconds;
        play.advance(game, fixedStep, frame, profile);
        return profile.simulationTicks;
    }
}

TEST_CASE("Running play advances the simulation by the fixed step", "[app][play-control]")
{
    advanced_platformer::Game running = tests::fixtureGame();
    advanced_platformer::FixedStep fixedStep;
    advanced_platformer::PlayControl play;

    REQUIRE_FALSE(play.paused());
    REQUIRE(advanceFrame(play, running, fixedStep) == TicksPerFrame);
    REQUIRE(fixedStep.pendingSeconds() > 0.0);
}

TEST_CASE("Pausing stops the simulation and drops the pending frame time", "[app][play-control]")
{
    advanced_platformer::Game running = tests::fixtureGame();
    advanced_platformer::FixedStep fixedStep;
    advanced_platformer::PlayControl play;
    advanceFrame(play, running, fixedStep);

    play.togglePause();

    REQUIRE(play.simulationPaused());
    REQUIRE(play.paused());
    REQUIRE(advanceFrame(play, running, fixedStep) == 0);
    REQUIRE(fixedStep.pendingSeconds() == 0.0);

    play.togglePause();
    advanceFrame(play, running, fixedStep);

    REQUIRE_FALSE(play.simulationPaused());
    REQUIRE(advanceFrame(play, running, fixedStep) == TicksPerFrame);
}

TEST_CASE("A step request while paused runs exactly one tick once", "[app][play-control]")
{
    advanced_platformer::Game running = tests::fixtureGame();
    advanced_platformer::FixedStep fixedStep;
    advanced_platformer::PlayControl play;
    play.togglePause();
    advanceFrame(play, running, fixedStep);

    play.requestStep();

    REQUIRE(advanceFrame(play, running, fixedStep) == 1);
    REQUIRE(advanceFrame(play, running, fixedStep) == 0);
}

TEST_CASE("A step request is ignored while play is running", "[app][play-control]")
{
    advanced_platformer::Game running = tests::fixtureGame();
    advanced_platformer::FixedStep fixedStep;
    advanced_platformer::PlayControl play;

    play.requestStep();

    REQUIRE(advanceFrame(play, running, fixedStep) == TicksPerFrame);

    play.togglePause();

    REQUIRE(advanceFrame(play, running, fixedStep) == 0);
}

TEST_CASE("An interrupt skips one frame of simulation", "[app][play-control]")
{
    advanced_platformer::Game running = tests::fixtureGame();
    advanced_platformer::FixedStep fixedStep;
    advanced_platformer::PlayControl play;
    advanceFrame(play, running, fixedStep);

    play.interrupt();

    REQUIRE(advanceFrame(play, running, fixedStep) == 0);
    REQUIRE(fixedStep.pendingSeconds() == 0.0);
    REQUIRE(advanceFrame(play, running, fixedStep) == TicksPerFrame);
}

TEST_CASE("An open inventory pauses play and blocks stepping", "[app][play-control]")
{
    advanced_platformer::Game running = tests::fixtureGame();
    advanced_platformer::FixedStep fixedStep;
    advanced_platformer::PlayControl play;

    play.toggleInventory();

    REQUIRE(play.inventoryOpen());
    REQUIRE(play.paused());
    REQUIRE(advanceFrame(play, running, fixedStep) == 0);

    play.togglePause();
    play.requestStep();

    REQUIRE(advanceFrame(play, running, fixedStep) == 0);

    play.toggleInventory();

    REQUIRE_FALSE(play.inventoryOpen());
    REQUIRE(play.paused());
}

TEST_CASE("A pause set from outside clears held input only when it changes", "[app][play-control]")
{
    advanced_platformer::PlayControl play;
    play.setButton(advanced_platformer::InputButton::Left, true);

    play.setPaused(true);

    REQUIRE(play.simulationPaused());
    REQUIRE_FALSE(play.input().isHeld(advanced_platformer::InputButton::Left));

    play.setButton(advanced_platformer::InputButton::Left, true);
    play.setPaused(true);

    REQUIRE(play.input().isHeld(advanced_platformer::InputButton::Left));

    play.setPaused(false);

    REQUIRE_FALSE(play.simulationPaused());
    REQUIRE_FALSE(play.input().isHeld(advanced_platformer::InputButton::Left));
}

TEST_CASE("Aim follows the cursor and keeps the last direction without one", "[app][play-control]")
{
    advanced_platformer::Game running = tests::fixtureGame();
    advanced_platformer::PlayControl play;
    const glm::vec2 aimedAtCursor = running.playerAimDirection(Cursor);
    REQUIRE(aimedAtCursor != glm::vec2{0.0F, 0.0F});

    REQUIRE(play.playerIntentions(running, {}).aimDirection == glm::vec2{1.0F, 0.0F});
    REQUIRE(play.playerIntentions(running, {Cursor}).aimDirection == aimedAtCursor);
    REQUIRE(play.playerIntentions(running, {}).aimDirection == aimedAtCursor);
}

TEST_CASE("Attacks reach the player only while the cursor is on the game", "[app][play-control]")
{
    advanced_platformer::Game running = tests::fixtureGame();
    advanced_platformer::PlayControl play;

    play.setButton(advanced_platformer::InputButton::PrimaryAttack, true);

    REQUIRE(play.playerIntentions(running, {Cursor}).primaryAttackPressed);

    play.setButton(advanced_platformer::InputButton::PrimaryAttack, false);
    play.setButton(advanced_platformer::InputButton::PrimaryAttack, true);

    REQUIRE_FALSE(play.playerIntentions(running, {Cursor, true}).primaryAttackPressed);

    play.setButton(advanced_platformer::InputButton::PrimaryAttack, false);
    play.setButton(advanced_platformer::InputButton::PrimaryAttack, true);
    play.toggleInventory();

    REQUIRE_FALSE(play.playerIntentions(running, {Cursor}).primaryAttackPressed);
}

TEST_CASE("Held movement is dropped while the UI captures the keyboard", "[app][play-control]")
{
    advanced_platformer::Game running = tests::fixtureGame();
    advanced_platformer::FixedStep fixedStep;
    advanced_platformer::PlayControl play;
    play.setButton(advanced_platformer::InputButton::Left, true);

    advanceFrame(play, running, fixedStep, {Cursor, false, true});

    REQUIRE_FALSE(play.input().isHeld(advanced_platformer::InputButton::Left));
}

TEST_CASE("Only attacks are dropped while the cursor is off the game", "[app][play-control]")
{
    advanced_platformer::Game running = tests::fixtureGame();
    advanced_platformer::FixedStep fixedStep;
    advanced_platformer::PlayControl play;
    play.setButton(advanced_platformer::InputButton::Left, true);
    play.setButton(advanced_platformer::InputButton::PrimaryAttack, true);

    advanceFrame(play, running, fixedStep, {Cursor, true, false});

    REQUIRE(play.input().isHeld(advanced_platformer::InputButton::Left));
    REQUIRE_FALSE(play.input().isHeld(advanced_platformer::InputButton::PrimaryAttack));
}

TEST_CASE("Paused play drops all held input", "[app][play-control]")
{
    advanced_platformer::Game running = tests::fixtureGame();
    advanced_platformer::FixedStep fixedStep;
    advanced_platformer::PlayControl play;
    play.setButton(advanced_platformer::InputButton::Left, true);
    play.togglePause();

    advanceFrame(play, running, fixedStep, {Cursor});

    REQUIRE_FALSE(play.input().isHeld(advanced_platformer::InputButton::Left));
}
