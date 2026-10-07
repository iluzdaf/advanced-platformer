#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "debug/debug_cursor.hpp"
#include "game/game.hpp"
#include "game/play_control.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "support/fixture_game.hpp"

namespace
{
    constexpr int ScanStep = 4;

    glm::vec2 scanCursor(const advanced_platformer::Game& game, bool overMachineActor)
    {
        for (int y = 0; y < advanced_platformer::InternalHeight; y += ScanStep)
        {
            for (int x = 0; x < advanced_platformer::InternalWidth; x += ScanStep)
            {
                const glm::vec2 cursor{static_cast<float>(x), static_cast<float>(y)};
                if (game.machineActorAt(cursor).has_value() == overMachineActor)
                {
                    return cursor;
                }
            }
        }
        throw std::logic_error("The fixture level shows no such cursor position");
    }

    glm::vec2 cursorOverMachineActor(const advanced_platformer::Game& game)
    {
        return scanCursor(game, true);
    }

    glm::vec2 cursorOverEmptySpace(const advanced_platformer::Game& game)
    {
        return scanCursor(game, false);
    }
}

TEST_CASE("A click on a machine actor selects it and drops pending attacks", "[app][debug-cursor]")
{
    advanced_platformer::Game game = tests::fixtureGame();
    advanced_platformer::PlayControl play;
    advanced_platformer::DebugCursor cursor;
    const glm::vec2 overActor = cursorOverMachineActor(game);
    play.setButton(advanced_platformer::InputButton::PrimaryAttack, true);

    selectClickedMachineActor(cursor, game, play, {overActor, true, false});

    REQUIRE(cursor.machineActor == game.machineActorAt(overActor));
    REQUIRE_FALSE(play.input().isHeld(advanced_platformer::InputButton::PrimaryAttack));
}

TEST_CASE("A click on the selected actor clears the selection", "[app][debug-cursor]")
{
    advanced_platformer::Game game = tests::fixtureGame();
    advanced_platformer::PlayControl play;
    advanced_platformer::DebugCursor cursor;
    const glm::vec2 overActor = cursorOverMachineActor(game);

    selectClickedMachineActor(cursor, game, play, {overActor, true, false});
    selectClickedMachineActor(cursor, game, play, {overActor, true, false});

    REQUIRE_FALSE(cursor.machineActor.has_value());
}

TEST_CASE("A click on empty space leaves the selection and attacks alone", "[app][debug-cursor]")
{
    advanced_platformer::Game game = tests::fixtureGame();
    advanced_platformer::PlayControl play;
    advanced_platformer::DebugCursor cursor;
    const glm::vec2 overActor = cursorOverMachineActor(game);
    selectClickedMachineActor(cursor, game, play, {overActor, true, false});
    play.setButton(advanced_platformer::InputButton::PrimaryAttack, true);

    selectClickedMachineActor(cursor, game, play, {cursorOverEmptySpace(game), true, false});

    REQUIRE(cursor.machineActor == game.machineActorAt(overActor));
    REQUIRE(play.input().isHeld(advanced_platformer::InputButton::PrimaryAttack));
}

TEST_CASE(
    "A click is ignored while the UI captures the mouse, without a press, or without a cursor",
    "[app][debug-cursor]")
{
    advanced_platformer::Game game = tests::fixtureGame();
    advanced_platformer::PlayControl play;
    advanced_platformer::DebugCursor cursor;
    const glm::vec2 overActor = cursorOverMachineActor(game);

    selectClickedMachineActor(cursor, game, play, {overActor, true, true});
    selectClickedMachineActor(cursor, game, play, {overActor, false, false});
    selectClickedMachineActor(cursor, game, play, {std::nullopt, true, false});

    REQUIRE_FALSE(cursor.machineActor.has_value());
}

TEST_CASE("A break request is consumed even when nothing breaks", "[app][debug-cursor]")
{
    advanced_platformer::Game game = tests::fixtureGame();
    advanced_platformer::DebugCursor cursor;
    cursor.breakTileRequested = true;

    REQUIRE_FALSE(breakRequestedTile(cursor, game, glm::vec2{0.0F, 0.0F}));
    REQUIRE_FALSE(cursor.breakTileRequested);
    REQUIRE_FALSE(breakRequestedTile(cursor, game, glm::vec2{0.0F, 0.0F}));
}

TEST_CASE("A break request without a cursor is dropped", "[app][debug-cursor]")
{
    advanced_platformer::Game game = tests::fixtureGame();
    advanced_platformer::DebugCursor cursor;
    cursor.breakTileRequested = true;

    REQUIRE_FALSE(breakRequestedTile(cursor, game, std::nullopt));
    REQUIRE_FALSE(cursor.breakTileRequested);
}
