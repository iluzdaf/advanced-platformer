#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_activity_scripts.hpp"
#include "lua_npc_scripts.hpp"
#include "lua_script_diagnostic.hpp"

namespace
{
    using advanced_platformer::ActorId;
    using advanced_platformer::LuaNpcScripts;
    using advanced_platformer::LuaScriptDiagnostic;
    using advanced_platformer::LuaScriptDiagnosticKind;
    using advanced_platformer::NpcActivity;
    using advanced_platformer::NpcActivitySnapshot;

    constexpr ActorId Actor{7};
    const NpcActivity Talk{"talker", "talk"};
}

TEST_CASE("A script's print is recorded with the call that printed it", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "talker",
        R"(
            print("loading", 2)
            return {activities = {talk = {
                description = 'Test activity talk',
                enter = function() print("hello") end,
                update = function(self, snapshot) print(snapshot.feet.x, true, nil) end,
            }}}
        )",
        "talker.lua");
    NpcActivitySnapshot snapshot;
    snapshot.feet = {4.0F, 0.0F};

    scripts.enter(Actor, Talk, snapshot);
    scripts.update(Actor, Talk, snapshot, 0.1F);

    const std::vector<LuaScriptDiagnostic>& printed = scripts.diagnostics();
    REQUIRE(printed.size() == 3);
    REQUIRE(printed[0].kind == LuaScriptDiagnosticKind::Print);
    REQUIRE(printed[0].message == "loading\t2");
    REQUIRE(printed[0].source == "talker.lua");
    REQUIRE(printed[0].script == "talker");
    REQUIRE(printed[0].hook == "load");
    REQUIRE_FALSE(printed[0].actor.has_value());
    REQUIRE(printed[1].message == "hello");
    REQUIRE(printed[1].activity == "talk");
    REQUIRE(printed[1].hook == "enter");
    REQUIRE(printed[1].actor == std::optional<ActorId>{Actor});
    REQUIRE(printed[2].message == "4.0\ttrue\tnil");
    REQUIRE(printed[2].hook == "update");
}

TEST_CASE("Taking the diagnostics empties them", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "talker",
        R"(return {activities = {talk = {description = 'Test activity talk', update = function() error("boom") end}}})");
    scripts.enter(Actor, Talk, {});
    scripts.update(Actor, Talk, {}, 0.1F);

    const std::vector<LuaScriptDiagnostic> taken = scripts.takeDiagnostics();
    REQUIRE(taken.size() == 1);
    REQUIRE(taken.front().kind == LuaScriptDiagnosticKind::Error);
    REQUIRE(taken.front().hook == "update");
    REQUIRE(scripts.diagnostics().empty());
    REQUIRE(scripts.takeDiagnostics().empty());
}
