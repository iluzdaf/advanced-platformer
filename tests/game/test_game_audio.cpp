#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <memory>
#include <utility>

#include "advanced_platformer/audio/sound_patch.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "content/actor_catalog.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "content/game_content.hpp"
#include "game.hpp"
#include "support/fixture_game.hpp"
#include "support/fixed_step.hpp"

TEST_CASE(
    "A presentation sound reaches the application once and retains its samples across reload",
    "[audio][game]")
{
    auto content = tests::fixtureContent();
    content.gameCatalogs.actors.definitions.at("test_player").primaryAttack =
        advanced_platformer::RangedWeapon{};
    const auto original = std::make_shared<const advanced_platformer::SoundBuffer>(
        advanced_platformer::SoundBuffer{{0.25F}, 44100});
    content.gameCatalogs.sounds.emplace("test", original);
    content.presentation.loadScriptText(
        R"(return {onShot = function() return {sound={name="test"}} end})", "audio.lua");
    auto game = tests::fixtureGame(std::move(content));
    advanced_platformer::InputIntentions shot;
    shot.primaryAttackPressed = true;
    shot.aimDirection = {1, 0};
    game.update(shot, tests::FixedStepSeconds);
    const auto pending = game.takeSounds();
    REQUIRE_FALSE(pending.empty());
    REQUIRE(pending.front() == original);
    REQUIRE(game.takeSounds().empty());
    auto replacement = tests::fixtureContent();
    replacement.gameCatalogs.actors.definitions.at("test_player").primaryAttack =
        advanced_platformer::RangedWeapon{};
    const auto updated = std::make_shared<const advanced_platformer::SoundBuffer>(
        advanced_platformer::SoundBuffer{{0.5F}, 44100});
    replacement.gameCatalogs.sounds.emplace("test", updated);
    replacement.presentation.loadScriptText(
        R"(return {onShot = function() return {sound={name="test"}} end})", "new-audio.lua");
    game.reload(std::move(replacement));
    REQUIRE(pending.front()->samples.front() == 0.25F);
    for (int index = 0; index < 30; ++index)
    {
        game.update({}, tests::FixedStepSeconds);
    }
    game.update(shot, tests::FixedStepSeconds);
    const auto next = game.takeSounds();
    REQUIRE_FALSE(next.empty());
    REQUIRE(next.front() == updated);
    REQUIRE(game.takeScriptDiagnostics().empty());
}

TEST_CASE("An unknown presentation sound is diagnosed without queuing audio", "[audio][game]")
{
    auto content = tests::fixtureContent();
    content.gameCatalogs.actors.definitions.at("test_player").primaryAttack =
        advanced_platformer::RangedWeapon{};
    content.presentation.loadScriptText(
        R"(return {onShot = function() return {sound={name="missing"}} end})", "audio.lua");
    auto game = tests::fixtureGame(std::move(content));
    advanced_platformer::InputIntentions shot;
    shot.primaryAttackPressed = true;
    shot.aimDirection = {1, 0};
    game.update(shot, tests::FixedStepSeconds);
    REQUIRE(game.takeSounds().empty());
    const auto diagnostics = game.takeScriptDiagnostics();
    REQUIRE_FALSE(diagnostics.empty());
    REQUIRE_THAT(
        diagnostics.front().message,
        Catch::Matchers::ContainsSubstring("not in the sound catalog"));
}
