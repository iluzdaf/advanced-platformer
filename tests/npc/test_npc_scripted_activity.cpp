#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <string>
#include <vector>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/actor/actor_system.hpp"
#include "advanced_platformer/combat/attack_system.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/navigation/navigation_fill.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_activity_scripts.hpp"
#include "advanced_platformer/npc/npc_scripted_activity.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/npc/npc_system.hpp"
#include "advanced_platformer/npc/npc_senses.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "advanced_platformer/world/world_requests.hpp"
#include "advanced_platformer/world/world_simulation.hpp"
#include "support/recording_npc_scripts.hpp"
#include "support/tile_map_builder.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/npc_machine_builder.hpp"
#include "support/fixed_step.hpp"
#include "support/tile_size.hpp"

using tests::actor;
using tests::brain;
using tests::machine;

namespace
{
    tests::ActorBuilder makePlayer(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F}).atFeet(feet).platforming();
    }

    tests::ActorBuilder::Thinking makeNpc(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet(feet)
            .flying(20.0F)
            .thinking({64.0F, 1.0F});
    }
}

TEST_CASE("A scripted route follows a climbing path", "[npc][lua][climb]")
{
    advanced_platformer::TileMap map = tests::TileMapBuilder({"..............",
                                                              "..cccccccccc..",
                                                              ".c..........c.",
                                                              ".c.########.c.",
                                                              ".c.########.c.",
                                                              ".c.########.c.",
                                                              "..##########..",
                                                              ".............."})
                                           .where('c', tests::Tile{}.blocksMovement().climbable());
    advanced_platformer::World world;
    advanced_platformer::Actor npc =
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .inCell({2, 5})
            .platforming()
            .climbing({60.0F})
            .thinking({})
            .running(
                tests::NpcMachineBuilder::named("climber").state(
                    "route", advanced_platformer::LuaNpcActivity{"climber", "route"}));
    tests::platformerMovement(npc).grounded = true;
    const advanced_platformer::ActorId npcId = world.addActor(npc);
    tests::RecordingNpcScripts scripts;
    scripts.command.routeTo = advanced_platformer::feetInCell(tests::TileSize, {11, 5});

    bool requestedClimb = false;
    bool reachedCeiling = false;
    for (int tick = 0; tick < 1500 && !reachedCeiling; ++tick)
    {
        advanced_platformer::updateWorldSimulation(
            map, world, tests::FixedStepSeconds, nullptr, &scripts);
        requestedClimb = requestedClimb || actor(world, npcId).intentions.climbGrip ==
                                               advanced_platformer::ClimbGrip::Hold;
        reachedCeiling =
            tests::surfaceClimb(world, npcId).surface == advanced_platformer::ClimbSurface::Ceiling;
    }
    REQUIRE(requestedClimb);
    REQUIRE(reachedCeiling);
}

TEST_CASE(
    "A scripted machine activity receives snapshots and returns engine commands",
    "[npc][lua]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "########"});
    advanced_platformer::World world;
    const advanced_platformer::ActorId npcId = world.addActor(
        makeNpc({24.0F, 32.0F})
            .running(
                tests::NpcMachineBuilder::named("scripted")
                    .state("roam", advanced_platformer::LuaNpcActivity{"rat", "roam"}))
            .patrolling({24.0F, 32.0F}, {72.0F, 32.0F}));
    tests::RecordingNpcScripts scripts;
    scripts.command.routeTo = glm::vec2{72.0F, 32.0F};
    scripts.command.aimAt = glm::vec2{80.0F, 16.0F};
    scripts.command.intentions.primaryAttackPressed = true;

    advanced_platformer::updateNpcBehaviour(map, world, 0.1F, &scripts);

    REQUIRE(scripts.calls.size() == 2);
    REQUIRE(scripts.calls[0].hook == "enter");
    REQUIRE(scripts.calls[1].hook == "update");
    REQUIRE(scripts.calls[0].actor == npcId);
    REQUIRE(scripts.calls[0].activity == advanced_platformer::LuaNpcActivity{"rat", "roam"});
    REQUIRE(scripts.calls[0].snapshot.feet == glm::vec2{24.0F, 32.0F});
    REQUIRE(scripts.calls[0].snapshot.facts.stateElapsed == 0.0F);
    REQUIRE(scripts.calls[1].snapshot.facts.stateElapsed == 0.0F);
    REQUIRE_FALSE(scripts.calls[0].snapshot.routeComplete);
    REQUIRE_FALSE(scripts.calls[0].snapshot.targetFeet.has_value());
    REQUIRE(scripts.calls[0].snapshot.patrol.has_value());
    const advanced_platformer::Patrol scriptedPatrol =
        scripts.calls[0].snapshot.patrol.value_or(advanced_platformer::Patrol{});
    REQUIRE(scriptedPatrol.firstFeet == glm::vec2{24.0F, 32.0F});
    REQUIRE(scriptedPatrol.secondFeet == glm::vec2{72.0F, 32.0F});
    REQUIRE(scripts.updateSteps == std::vector<float>{0.1F});
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);
    REQUIRE(actor(world, npcId).intentions.aimDirection == glm::vec2{56.0F, -16.0F});
    REQUIRE(actor(world, npcId).intentions.primaryAttackPressed);
    REQUIRE(machine(world, npcId).stateElapsed == 0.1F);
    REQUIRE(brain(world, npcId).stateElapsed == 0.0F);

    advanced_platformer::updateNpcBehaviour(map, world, 0.1F, &scripts);
    REQUIRE(scripts.calls.size() == 3);
    REQUIRE(scripts.calls.back().hook == "update");
    REQUIRE(scripts.calls.back().snapshot.facts.stateElapsed == 0.1F);
}

TEST_CASE("A scripted machine exits and enters around a transition", "[npc][lua]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    advanced_platformer::World world;
    const advanced_platformer::ActorId playerId =
        tests::addPlayer(world, makePlayer({56.0F, 32.0F}));
    const advanced_platformer::ActorId npcId = world.addActor(
        makeNpc({24.0F, 32.0F})
            .running(
                tests::NpcMachineBuilder::named("scripted")
                    .state("waiting", advanced_platformer::LuaNpcActivity{"rat", "wait"})
                    .state("moving", advanced_platformer::LuaNpcActivity{"rat", "move"})
                    .transition("waiting", "moving")
                    .when("targetKnown", true)));
    tests::RecordingNpcScripts scripts;

    advanced_platformer::updateNpcBehaviour(map, world, 0.1F, &scripts);
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = {56.0F, 32.0F};
    tests::perception(world, npcId).targetVisible = true;
    advanced_platformer::updateNpcBehaviour(map, world, 0.1F, &scripts);

    REQUIRE(scripts.calls.size() == 5);
    REQUIRE(scripts.calls[0].hook == "enter");
    REQUIRE(scripts.calls[0].activity.activity == "wait");
    REQUIRE(scripts.calls[1].hook == "update");
    REQUIRE(scripts.calls[2].hook == "exit");
    REQUIRE(scripts.calls[2].activity.activity == "wait");
    REQUIRE(scripts.calls[2].snapshot.facts.stateElapsed == 0.1F);
    REQUIRE(scripts.calls[3].hook == "enter");
    REQUIRE(scripts.calls[3].activity.activity == "move");
    REQUIRE(scripts.calls[3].snapshot.facts.stateElapsed == 0.0F);
    REQUIRE(scripts.calls[3].snapshot.targetFeet == glm::vec2{56.0F, 32.0F});
    REQUIRE(scripts.calls[4].hook == "update");
    REQUIRE(scripts.calls[4].snapshot.facts.stateElapsed == 0.0F);
    REQUIRE(advanced_platformer::activeNpcMachineState(machine(world, npcId)).name == "moving");
}

TEST_CASE("A scripted machine activity requires a scripting runtime", "[npc][lua][validation]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"...", "...", "###"});
    advanced_platformer::World world;
    world.addActor(
        makeNpc({24.0F, 32.0F})
            .running(
                tests::NpcMachineBuilder::named("scripted")
                    .state("waiting", advanced_platformer::LuaNpcActivity{"rat", "wait"})));

    REQUIRE_THROWS_WITH(
        advanced_platformer::updateNpcBehaviour(map, world, 0.1F),
        "A scripted NPC activity needs the scripting runtime");
}

TEST_CASE("Removing an actor forgets its scripted activity state", "[npc][lua][lifecycle]")
{
    advanced_platformer::World world;
    const advanced_platformer::ActorId npcId = world.addActor(makeNpc({24.0F, 32.0F}));
    advanced_platformer::WorldRequests requests;
    requests.remove(npcId);
    tests::RecordingNpcScripts scripts;

    advanced_platformer::forgetScriptedActivities(requests.actorsToRemove(), scripts);
    REQUIRE(world.findActor(npcId) != nullptr);
    advanced_platformer::applyWorldRequests(world, requests);

    REQUIRE(scripts.forgotten == std::vector<advanced_platformer::ActorId>{npcId});
    REQUIRE(world.findActor(npcId) == nullptr);
}
