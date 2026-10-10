#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/render/presentation_scripts.hpp"
#include "advanced_platformer/world/world.hpp"
#include "lua_presentation_script.hpp"

TEST_CASE("A Lua hook can return a catalog sound together with camera shake", "[audio][lua]")
{
    advanced_platformer::LuaPresentationScript script;
    script.setSoundNames({"test"});
    script.loadScriptText(
        R"(return {onShot=function() return {sound={name="test"},shake={duration=0.1,magnitude=1}} end})",
        "audio.lua");
    const auto effects = script.onEvent(
        {advanced_platformer::ActorId{1}, {}, advanced_platformer::WorldEventKind::Shot}, true);
    REQUIRE(effects.sound.has_value());
    REQUIRE(effects.sound.value_or(advanced_platformer::SoundEffect{}).name == "test");
    REQUIRE(effects.shake.has_value());
    REQUIRE(script.diagnostics().empty());
}

TEST_CASE("Sound effects without a hook ask for nothing", "[audio][lua]")
{
    advanced_platformer::LuaPresentationScript script;
    script.loadScriptText("return {}", "audio.lua");
    REQUIRE_FALSE(script.onEvent({{}, {}, advanced_platformer::WorldEventKind::Shot}, true)
                      .sound.has_value());
    REQUIRE(script.diagnostics().empty());
}

TEST_CASE("Invalid Lua sound effects are ignored and identify the source and hook", "[audio][lua]")
{
    std::string effect;
    std::string message;
    SECTION("Unknown sound")
    {
        effect = "{name='missing'}";
        message = "not in the sound catalog";
    }
    SECTION("Empty name")
    {
        effect = "{name=''}";
        message = "non-empty string";
    }
    SECTION("Missing name")
    {
        effect = "{}";
        message = "non-empty string";
    }
    SECTION("Numeric name")
    {
        effect = "{name=4}";
        message = "non-empty string";
    }
    SECTION("Not a table")
    {
        effect = "3";
        message = "sound must be a table";
    }
    SECTION("Extra field")
    {
        effect = "{name='test',gain=1}";
        message = "unknown field 'gain'";
    }
    advanced_platformer::LuaPresentationScript script;
    script.setSoundNames({"test"});
    script.loadScriptText(
        "return {onShot=function() return {sound=" + effect + "} end}", "audio.lua");
    REQUIRE_FALSE(script.onEvent({{}, {}, advanced_platformer::WorldEventKind::Shot}, false)
                      .sound.has_value());
    const auto diagnostics = script.takeDiagnostics();
    REQUIRE(diagnostics.size() == 1);
    REQUIRE(diagnostics.front().source == "audio.lua");
    REQUIRE(diagnostics.front().hook == "onShot");
    REQUIRE_THAT(diagnostics.front().message, Catch::Matchers::ContainsSubstring(message));
}
