#include <catch2/catch_test_macros.hpp>

#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "support/npc_machine_builder.hpp"

using tests::NpcMachineBuilder;

TEST_CASE(
    "The machine builder keeps states and transitions in the order stated",
    "[support][machine-builder]")
{
    const advanced_platformer::NpcStateMachine machine =
        NpcMachineBuilder::named("test")
            .state("rest", tests::testActivity("idle"))
            .state("hunt", tests::testActivity("chase"))
            .transition("rest", "hunt")
            .when("targetKnown", true)
            .transition("hunt", "rest")
            .when("targetKnown", false)
            .after(0.5F);
    REQUIRE(machine.name == "test");
    REQUIRE(machine.states.size() == 2);
    REQUIRE(machine.states[1].name == "hunt");
    REQUIRE(machine.states[1].does == advanced_platformer::NpcActivity{"test", "chase"});
    REQUIRE(machine.transitions.size() == 2);
    REQUIRE(machine.transitions[0].after == 0.0F);
    REQUIRE(machine.transitions[1].from == "hunt");
    REQUIRE(machine.transitions[1].when.at("targetKnown") == false);
    REQUIRE(machine.transitions[1].after == 0.5F);
}
