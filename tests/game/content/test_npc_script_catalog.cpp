#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <utility>

#include "content/machine_catalog.hpp"
#include "content/npc_script_catalog.hpp"
#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "lua_npc_scripts.hpp"

using Catch::Matchers::ContainsSubstring;

namespace
{
    advanced_platformer::MachineCatalog catalogWith(advanced_platformer::NpcActivity activity)
    {
        advanced_platformer::NpcStateMachine machine;
        machine.name = "test";
        machine.states.push_back({"waiting", std::move(activity)});
        return {{machine.name, std::move(machine)}};
    }

    advanced_platformer::MachineCatalog catalogAsking(
        advanced_platformer::NpcActivity activity,
        const char* fact)
    {
        advanced_platformer::MachineCatalog machines = catalogWith(std::move(activity));
        advanced_platformer::NpcStateMachine& machine = machines.begin()->second;
        machine.states.push_back({"other", machine.states.front().does});
        machine.transitions.push_back({"waiting", "other", {{fact, true}}});
        return machines;
    }
}

TEST_CASE("Machine scripts load from the content scripts directory", "[app][machines][lua]")
{
    advanced_platformer::LuaNpcScripts scripts;
    const auto machines = catalogWith({"example_npc", "idle"});

    advanced_platformer::loadNpcActivityScripts(scripts, machines, "tests/fixtures/scripts");

    REQUIRE(scripts.hasActivity({"example_npc", "idle"}));
}

TEST_CASE("A machine's transitions may ask the facts of its scripts", "[app][machines][lua]")
{
    advanced_platformer::LuaNpcScripts scripts;
    const auto machines = catalogAsking({"answering_npc", "idle"}, "nearTarget");

    advanced_platformer::loadNpcActivityScripts(scripts, machines, "tests/fixtures/scripts");

    REQUIRE(scripts.hasFacts("answering_npc"));
}

TEST_CASE("Machine script loading rejects unresolved and unsafe references", "[app][machines][lua]")
{
    advanced_platformer::LuaNpcScripts scripts;

    SECTION("Unknown activity")
    {
        const auto machines = catalogWith({"example_npc", "missing"});
        REQUIRE_THROWS_WITH(
            advanced_platformer::loadNpcActivityScripts(
                scripts, machines, "tests/fixtures/scripts"),
            ContainsSubstring("test") && ContainsSubstring("example_npc.missing"));
    }
    SECTION("A script name cannot escape the scripts directory")
    {
        const auto machines = catalogWith({"../example_npc", "idle"});
        REQUIRE_THROWS_WITH(
            advanced_platformer::loadNpcActivityScripts(
                scripts, machines, "tests/fixtures/scripts"),
            ContainsSubstring("must be a file stem"));
    }
    SECTION("A condition neither the engine nor the machine's scripts answer")
    {
        const auto machines = catalogAsking({"example_npc", "idle"}, "nearTarget");
        REQUIRE_THROWS_WITH(
            advanced_platformer::loadNpcActivityScripts(
                scripts, machines, "tests/fixtures/scripts"),
            ContainsSubstring("'nearTarget', which neither the engine nor its scripts answer"));
    }
    SECTION("Two scripts of one machine answering the same fact")
    {
        auto machines = catalogAsking({"answering_npc", "idle"}, "nearTarget");
        machines.begin()->second.states.back().does = {"twin_answering_npc", "idle"};
        REQUIRE_THROWS_WITH(
            advanced_platformer::loadNpcActivityScripts(
                scripts, machines, "tests/fixtures/scripts"),
            ContainsSubstring("the fact 'nearTarget' in both"));
    }
}
