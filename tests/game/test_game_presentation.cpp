#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "game.hpp"
#include "lua_presentation_script.hpp"
#include "lua_script_diagnostic.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/render/camera.hpp"
#include "support/fixed_step.hpp"
#include "support/fixture_game.hpp"

namespace
{
    constexpr int MaximumSteps = 600;

    advanced_platformer::Game gameWith(const char* presentationSource)
    {
        advanced_platformer::LuaPresentationScript presentation;
        presentation.loadScriptText(presentationSource, "fx.lua");
        advanced_platformer::GameContent content =
            tests::fixtureContent("tests/fixtures/rooms/finish/pieces.json");
        content.presentation = std::move(presentation);
        return tests::fixtureGame(std::move(content));
    }

    bool shaking(const advanced_platformer::Game& game)
    {
        return game.renderCamera().position != game.currentCamera().position;
    }

    void jump(advanced_platformer::Game& game)
    {
        advanced_platformer::InputIntentions intentions;
        intentions.jumpPressed = true;
        intentions.jumpHeld = true;
        game.update(intentions, tests::FixedStepSeconds);
    }

    bool shakesWithin(advanced_platformer::Game& game, int steps)
    {
        for (int step = 0; step < steps; ++step)
        {
            game.update({}, tests::FixedStepSeconds);
            if (shaking(game))
            {
                return true;
            }
        }
        return false;
    }
}

TEST_CASE(
    "A landing shakes the rendered camera when the script asks, then it settles",
    "[app][presentation]")
{
    advanced_platformer::Game game = gameWith(
        R"(return { onLanding = function(event)
            if event.actor == "player" then return { shake = { duration = 0.5, magnitude = 3 } } end
        end })");
    REQUIRE_FALSE(shaking(game));

    jump(game);
    REQUIRE(shakesWithin(game, MaximumSteps));

    for (int step = 0; step < MaximumSteps; ++step)
    {
        game.update({}, tests::FixedStepSeconds);
    }
    REQUIRE_FALSE(shaking(game));
    REQUIRE(game.takeScriptDiagnostics().empty());
}

TEST_CASE("Without a script or a hook the camera never shakes", "[app][presentation]")
{
    advanced_platformer::Game game = gameWith("return {}");

    jump(game);
    REQUIRE_FALSE(shakesWithin(game, MaximumSteps));
}

TEST_CASE("A bad effect from the script reaches the game's diagnostics", "[app][presentation]")
{
    advanced_platformer::Game game = gameWith(
        R"(return { onLanding = function(event) return { shake = { duration = 0, magnitude = 3 } } end })");

    jump(game);
    REQUIRE_FALSE(shakesWithin(game, MaximumSteps));
    const std::vector<advanced_platformer::LuaScriptDiagnostic> reported =
        game.takeScriptDiagnostics();
    REQUIRE(reported.size() >= 1);
    REQUIRE(reported.front().script == "presentation");
    REQUIRE_THAT(
        reported.front().message,
        Catch::Matchers::ContainsSubstring("shake.duration must be greater than zero"));
}
