#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>

#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_activity_scripts.hpp"
#include "lua_npc_scripts.hpp"

namespace
{
    constexpr advanced_platformer::ActorId FirstActor{1};
    const advanced_platformer::NpcActivity Activity{"example", "decide"};
}

TEST_CASE("A Lua script can be loaded from an asset file", "[lua][npc]")
{
    advanced_platformer::LuaNpcScripts scripts;
    scripts.loadScript("fixture", "tests/fixtures/scripts/example_npc.lua");
    const advanced_platformer::NpcActivity activity{"fixture", "idle"};
    advanced_platformer::NpcActivitySnapshot snapshot;
    snapshot.facts.targetKnown = true;
    scripts.enter(FirstActor, activity, snapshot);

    const advanced_platformer::NpcActivityCommand command =
        scripts.update(FirstActor, activity, snapshot, 0.25F);

    REQUIRE(command.intentions.direction.x == 1.0F);
    REQUIRE(command.intentions.primaryAttackPressed);
}

TEST_CASE("Lua scripts reject activities without an update function", "[lua][npc]")
{
    advanced_platformer::LuaNpcScripts scripts;

    REQUIRE_THROWS_WITH(
        scripts.loadScriptText(
            "example", "return {activities={wait={enter=function() end}}}", "missing.lua"),
        Catch::Matchers::ContainsSubstring("example.wait"));
}

TEST_CASE("A Lua script that cannot be read is reported by its full path", "[lua][npc]")
{
    advanced_platformer::LuaNpcScripts scripts;
    const std::filesystem::path missing = "missing-script.lua";
    REQUIRE_THROWS_WITH(
        scripts.loadScript("missing", missing),
        Catch::Matchers::ContainsSubstring(std::filesystem::absolute(missing).string()));
}

TEST_CASE("A broken reload leaves the working Lua script in place", "[lua][npc]")
{
    advanced_platformer::LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example",
        "return {activities={decide={update=function() return {jumpPressed=true} end}}}",
        "working.lua");

    REQUIRE_THROWS_WITH(
        scripts.loadScriptText("example", "return {", "broken.lua"),
        Catch::Matchers::ContainsSubstring("broken.lua"));
    REQUIRE(scripts.hasActivity(Activity));

    const advanced_platformer::NpcActivitySnapshot snapshot;
    scripts.enter(FirstActor, Activity, snapshot);
    REQUIRE(scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.jumpPressed);
}
