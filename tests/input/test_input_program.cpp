#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/input/input_program.hpp"

TEST_CASE("An input program replays consecutive intentions", "[input][program]")
{
    advanced_platformer::InputIntentions jump;
    jump.jumpPressed = true;
    jump.jumpHeld = true;
    advanced_platformer::InputIntentions right;
    right.direction.x = 1.0F;
    const advanced_platformer::InputProgram program{{0.1F, jump}, {0.2F, right}};

    REQUIRE(advanced_platformer::durationOf(program) == 0.3F);
    REQUIRE(advanced_platformer::replayInput(program, 0.0F).jumpPressed);
    REQUIRE(advanced_platformer::replayInput(program, 0.1F).direction.x == 1.0F);
    REQUIRE(advanced_platformer::replayInput(program, 0.3F).direction.x == 0.0F);
}

TEST_CASE("Input programs reject invalid time", "[input][program]")
{
    REQUIRE_THROWS_AS(advanced_platformer::durationOf({{0.0F, {}}}), std::invalid_argument);
    REQUIRE_THROWS_AS(advanced_platformer::replayInput({}, -0.1F), std::invalid_argument);
}
