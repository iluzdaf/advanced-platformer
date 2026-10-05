#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/render/presentation_scripts.hpp"
#include "advanced_platformer/world/world.hpp"
#include "lua_presentation_script.hpp"
#include "lua_script_diagnostic.hpp"

namespace
{
    using advanced_platformer::ActorId;
    using advanced_platformer::LuaPresentationScript;
    using advanced_platformer::LuaScriptDiagnostic;
    using advanced_platformer::LuaScriptDiagnosticKind;
    using advanced_platformer::PresentationEffects;
    using advanced_platformer::WorldEvent;
    using advanced_platformer::WorldEventKind;

    constexpr ActorId Hero{7};

    WorldEvent knockback()
    {
        return {Hero, {40.0F, 64.0F}, WorldEventKind::Knockback, {90.0F, -60.0F}};
    }
}

TEST_CASE("A presentation hook reads the event and returns its effects", "[lua][presentation]")
{
    LuaPresentationScript script;
    script.loadScriptText(
        R"(
            return {
                onKnockback = function(event)
                    if event.actor ~= "player" then return nil end
                    return { shake = { duration = 0.15, magnitude = event.velocity.x / 45 } }
                end,
            }
        )",
        "fx.lua");

    const PresentationEffects player = script.onEvent(knockback(), true);
    const advanced_platformer::CameraShakeEffect shake =
        player.shake.value_or(advanced_platformer::CameraShakeEffect{});
    REQUIRE(shake.duration == 0.15F);
    REQUIRE(shake.magnitude == 2.0F);

    const PresentationEffects npc = script.onEvent(knockback(), false);
    REQUIRE_FALSE(npc.shake.has_value());
    REQUIRE(script.diagnostics().empty());
}

TEST_CASE("Events without a hook, or without a script, ask for nothing", "[lua][presentation]")
{
    LuaPresentationScript script;
    REQUIRE_FALSE(script.loaded());
    REQUIRE_FALSE(script.onEvent(knockback(), true).shake.has_value());

    script.loadScriptText(
        R"(return { onShot = function(event) return { shake = { duration = 1, magnitude = 1 } } end })",
        "fx.lua");
    REQUIRE(script.loaded());
    REQUIRE_FALSE(script.onEvent(knockback(), true).shake.has_value());
    REQUIRE(script.onEvent({Hero, {0.0F, 0.0F}, WorldEventKind::Shot}, true).shake.has_value());
    REQUIRE(script.diagnostics().empty());
}

TEST_CASE("Bad effects are reported with their hook and ignored", "[lua][presentation]")
{
    LuaPresentationScript script;
    const char* effects = "";
    const char* expected = "";
    SECTION("A magnitude of zero")
    {
        effects = "{ shake = { duration = 0.1, magnitude = 0 } }";
        expected = "shake.magnitude must be greater than zero";
    }
    SECTION("An unknown effect")
    {
        effects = "{ flash = {} }";
        expected = "presentation effects has unknown field 'flash'";
    }
    SECTION("Effects that are not a table")
    {
        effects = "4";
        expected = "presentation effects must be a table or nil";
    }
    script.loadScriptText(
        std::string("return { onKnockback = function(event) return ") + effects + " end }",
        "fx.lua");

    REQUIRE_FALSE(script.onEvent(knockback(), true).shake.has_value());
    const std::vector<LuaScriptDiagnostic> reported = script.takeDiagnostics();
    REQUIRE(reported.size() == 1);
    REQUIRE(reported.front().kind == LuaScriptDiagnosticKind::Error);
    REQUIRE(reported.front().script == "presentation");
    REQUIRE(reported.front().hook == "onKnockback");
    REQUIRE(reported.front().source == "fx.lua");
    REQUIRE_THAT(reported.front().message, Catch::Matchers::ContainsSubstring(expected));
    REQUIRE(script.diagnostics().empty());
}

TEST_CASE("A hook that fails is reported and asks for nothing", "[lua][presentation]")
{
    LuaPresentationScript script;
    script.loadScriptText(
        R"(return { onKnockback = function(event) print("hit", event.kind) error("boom") end })",
        "fx.lua");

    REQUIRE_FALSE(script.onEvent(knockback(), true).shake.has_value());
    const std::vector<LuaScriptDiagnostic> reported = script.takeDiagnostics();
    REQUIRE(reported.size() == 2);
    REQUIRE(reported.front().kind == LuaScriptDiagnosticKind::Print);
    REQUIRE(reported.front().message == "hit\tknockback");
    REQUIRE(reported.back().kind == LuaScriptDiagnosticKind::Error);
    REQUIRE_THAT(reported.back().message, Catch::Matchers::ContainsSubstring("boom"));
    REQUIRE(reported.back().hook == "onKnockback");
}

TEST_CASE("Loading rejects anything but a table of known hook functions", "[lua][presentation]")
{
    LuaPresentationScript script;

    REQUIRE_THROWS_WITH(
        script.loadScriptText("return 1", "fx.lua"),
        Catch::Matchers::ContainsSubstring("must return a table of hooks"));
    REQUIRE_THROWS_WITH(
        script.loadScriptText("return { onHit = function() end }", "fx.lua"),
        Catch::Matchers::ContainsSubstring("unknown field 'onHit'"));
    REQUIRE_THROWS_WITH(
        script.loadScriptText("return { onShot = 3 }", "fx.lua"),
        Catch::Matchers::ContainsSubstring("onShot value that is not a function"));
    REQUIRE_THROWS_WITH(
        script.loadScriptText("return {", "fx.lua"), Catch::Matchers::ContainsSubstring("fx.lua"));
    REQUIRE_FALSE(script.loaded());
}
