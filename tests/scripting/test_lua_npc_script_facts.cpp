#include <map>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_activity_scripts.hpp"
#include "lua_npc_scripts.hpp"

namespace
{
    using advanced_platformer::ActorId;
    using advanced_platformer::LuaNpcScripts;
    using advanced_platformer::NpcActivity;
    using advanced_platformer::NpcActivitySnapshot;
    using Catch::Matchers::ContainsSubstring;

    constexpr ActorId FirstActor{1};
    constexpr ActorId SecondActor{2};

    const char* const CountingScript = R"(
                return {
                    facts = {
                        second = function(memory)
                            memory.order = memory.order .. "b"
                            return memory.steps >= 2
                        end,
                        first = function(memory, snapshot, step)
                            memory.steps = (memory.steps or 0) + 1
                            memory.order = (memory.order or "") .. "a"
                            return snapshot.facts.targetKnown and step > 0
                        end,
                    },
                    activities = {
                        count = {
                            description = 'Test activity count',
                            update = function(_, _, _, memory)
                                memory.counted = (memory.counted or 0) + 1
                                return { jumpPressed = memory.counted == 2, jumpHeld = memory.order == "abab" }
                            end,
                        },
                        check = {
                            description = 'Test activity check',
                            enter = function(_, _, memory)
                                memory.entered = memory.counted
                            end,
                            update = function(_, _, _, memory)
                                return { jumpPressed = memory.entered == 1 }
                            end,
                        },
                    },
                }
            )";

    LuaNpcScripts countingScripts()
    {
        LuaNpcScripts scripts;
        scripts.loadScriptText("counter", CountingScript, "counter.lua");
        return scripts;
    }
}

TEST_CASE("Script facts answer in name order and share one memory per actor", "[lua][npc]")
{
    LuaNpcScripts scripts = countingScripts();
    NpcActivitySnapshot snapshot;
    snapshot.facts.targetKnown = true;

    REQUIRE(scripts.hasFacts("counter"));
    REQUIRE(scripts.factNames("counter") == std::vector<std::string>{"first", "second"});
    REQUIRE(
        scripts.facts(FirstActor, "counter", snapshot, 0.1F) ==
        std::map<std::string, bool>{{"first", true}, {"second", false}});
    REQUIRE(
        scripts.facts(FirstActor, "counter", snapshot, 0.1F) ==
        std::map<std::string, bool>{{"first", true}, {"second", true}});
    REQUIRE(
        scripts.facts(SecondActor, "counter", snapshot, 0.0F) ==
        std::map<std::string, bool>{{"first", false}, {"second", false}});
    REQUIRE(scripts.diagnostics().empty());
}

TEST_CASE("A script's activities share its facts' memory across states", "[lua][npc]")
{
    LuaNpcScripts scripts = countingScripts();
    const NpcActivity count{"counter", "count"};
    const NpcActivity check{"counter", "check"};
    const NpcActivitySnapshot snapshot;

    scripts.facts(FirstActor, "counter", snapshot, 0.1F);
    scripts.facts(FirstActor, "counter", snapshot, 0.1F);
    scripts.enter(FirstActor, count, snapshot);
    REQUIRE_FALSE(scripts.update(FirstActor, count, snapshot, 0.1F).intentions.jumpPressed);
    scripts.exit(FirstActor, count, snapshot);
    scripts.enter(FirstActor, count, snapshot);
    const auto second = scripts.update(FirstActor, count, snapshot, 0.1F).intentions;
    REQUIRE(second.jumpPressed);
    REQUIRE(second.jumpHeld);

    scripts.enter(SecondActor, check, snapshot);
    REQUIRE_FALSE(scripts.update(SecondActor, check, snapshot, 0.1F).intentions.jumpPressed);
    REQUIRE(scripts.diagnostics().empty());
}

TEST_CASE("Forgetting an actor or reloading its script drops its script memory", "[lua][npc]")
{
    LuaNpcScripts scripts = countingScripts();
    const NpcActivitySnapshot snapshot;
    scripts.facts(FirstActor, "counter", snapshot, 0.1F);
    scripts.facts(FirstActor, "counter", snapshot, 0.1F);

    SECTION("Forgetting the actor")
    {
        scripts.forget(FirstActor);
    }
    SECTION("Reloading the script")
    {
        scripts.loadScriptText("counter", CountingScript, "counter.lua");
    }

    REQUIRE_FALSE(scripts.facts(FirstActor, "counter", snapshot, 0.1F).at("second"));
}

TEST_CASE("A script fact that fails or answers with no boolean is reported and fails", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "broken",
        R"(
            return {
                facts = {
                    failing = function() error("no answer") end,
                    vague = function() return 1 end,
                },
                activities = { idle = { description = 'Test activity idle', update = function() return nil end } },
            }
        )",
        "broken.lua");

    REQUIRE(
        scripts.facts(FirstActor, "broken", {}, 0.1F) ==
        std::map<std::string, bool>{{"failing", false}, {"vague", false}});
    REQUIRE(scripts.diagnostics().size() == 2);
    REQUIRE(scripts.diagnostics()[0].activity == "failing");
    REQUIRE(scripts.diagnostics()[0].hook == "fact");
    REQUIRE_THAT(scripts.diagnostics()[0].message, ContainsSubstring("no answer"));
    REQUIRE_THAT(scripts.diagnostics()[1].message, ContainsSubstring("true or false"));
}

TEST_CASE("A script without facts answers none", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "plain",
        "return { activities = { idle = { description = 'Test activity idle', update = function() "
        "end } } }",
        "plain.lua");

    REQUIRE_FALSE(scripts.hasFacts("plain"));
    REQUIRE_FALSE(scripts.hasFacts("missing"));
    REQUIRE(scripts.facts(FirstActor, "plain", {}, 0.1F).empty());
}

TEST_CASE("Script facts must be named functions that the engine does not answer", "[lua][npc]")
{
    LuaNpcScripts scripts;
    const char* source = "";
    const char* expected = "";
    SECTION("Facts that are not a table")
    {
        source = "return { facts = 1, activities = { idle = { description = 'Test activity idle', "
                 "update = function() end } } }";
        expected = "facts value that is not a table";
    }
    SECTION("A fact that is not a function")
    {
        source = "return { facts = { near = true }, activities = { idle = { description = 'Test "
                 "activity idle', update = function() "
                 "end } } }";
        expected = "fact 'near' that is not a function";
    }
    SECTION("A fact the engine already answers")
    {
        source = "return { facts = { targetKnown = function() return true end }, "
                 "activities = { idle = { description = 'Test activity idle', update = function() "
                 "end } } }";
        expected = "fact 'targetKnown' that the engine already answers";
    }
    REQUIRE_THROWS_WITH(
        scripts.loadScriptText("bad", source, "bad.lua"), ContainsSubstring(expected));
}
