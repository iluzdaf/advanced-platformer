#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <optional>
#include <string>
#include <vector>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/actor/actor_system.hpp"
#include "advanced_platformer/combat/attack_system.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/navigation/navigation_fill.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/navigation/navigation_path.hpp"
#include "advanced_platformer/npc/npc_activity_scripts.hpp"
#include "advanced_platformer/npc/npc_activity_runner.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/npc/npc_system.hpp"
#include "advanced_platformer/npc/npc_senses.hpp"
#include "advanced_platformer/world/level_exit.hpp"
#include "advanced_platformer/world/pickup.hpp"
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
                    "route", advanced_platformer::NpcActivity{"climber", "route"}));
    tests::component<advanced_platformer::PlatformerMovement>(npc).grounded = true;
    const advanced_platformer::ActorId npcId = world.addActor(npc);
    tests::RecordingNpcScripts scripts;
    scripts.command.routeTo = advanced_platformer::feetInCell(tests::TileSize, {11, 5});

    bool requestedClimb = false;
    bool reachedCeiling = false;
    for (int tick = 0; tick < 1500 && !reachedCeiling; ++tick)
    {
        advanced_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds, scripts);
        requestedClimb = requestedClimb || actor(world, npcId).intentions.climbGrip ==
                                               advanced_platformer::ClimbGrip::Hold;
        reachedCeiling =
            tests::component<advanced_platformer::SurfaceClimb>(world, npcId).surface ==
            advanced_platformer::ClimbSurface::Ceiling;
    }
    REQUIRE(requestedClimb);
    REQUIRE(reachedCeiling);
}

TEST_CASE("An NPC activity receives snapshots and returns engine commands", "[npc][lua]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "########"});
    advanced_platformer::World world;
    const advanced_platformer::ActorId npcId =
        world.addActor(makeNpc({24.0F, 32.0F})
                           .running(
                               tests::NpcMachineBuilder::named("scripted")
                                   .state("roam", advanced_platformer::NpcActivity{"rat", "roam"}))
                           .patrolling({24.0F, 32.0F}, {72.0F, 32.0F}));
    tests::RecordingNpcScripts scripts;
    scripts.command.routeTo = glm::vec2{72.0F, 32.0F};
    scripts.command.aimAt = glm::vec2{80.0F, 16.0F};
    scripts.command.intentions.primaryAttackPressed = true;

    advanced_platformer::updateNpcBehaviour(map, world, 0.1F, scripts);

    REQUIRE(scripts.calls.size() == 2);
    REQUIRE(scripts.calls[0].hook == "enter");
    REQUIRE(scripts.calls[1].hook == "update");
    REQUIRE(scripts.calls[0].actor == npcId);
    REQUIRE(scripts.calls[0].activity == advanced_platformer::NpcActivity{"rat", "roam"});
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
    REQUIRE(tests::component<advanced_platformer::NpcMachine>(world, npcId).stateElapsed == 0.1F);

    advanced_platformer::updateNpcBehaviour(map, world, 0.1F, scripts);
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
                    .state("waiting", advanced_platformer::NpcActivity{"rat", "wait"})
                    .state("moving", advanced_platformer::NpcActivity{"rat", "move"})
                    .transition("waiting", "moving")
                    .when("targetKnown", true)));
    tests::RecordingNpcScripts scripts;

    advanced_platformer::updateNpcBehaviour(map, world, 0.1F, scripts);
    tests::component<advanced_platformer::NpcBrain>(world, npcId).target = playerId;
    tests::component<advanced_platformer::NpcBrain>(world, npcId).lastKnownTargetFeet = {
        56.0F, 32.0F};
    tests::component<advanced_platformer::NpcPerception>(world, npcId).targetVisible = true;
    advanced_platformer::updateNpcBehaviour(map, world, 0.1F, scripts);

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
    REQUIRE(
        advanced_platformer::activeNpcMachineState(
            tests::component<advanced_platformer::NpcMachine>(world, npcId))
            .name == "moving");
}

TEST_CASE("Removing an actor forgets its activity state", "[npc][lua][lifecycle]")
{
    advanced_platformer::World world;
    const advanced_platformer::ActorId npcId = world.addActor(makeNpc({24.0F, 32.0F}));
    advanced_platformer::WorldRequests requests;
    requests.remove(npcId);
    tests::RecordingNpcScripts scripts;

    advanced_platformer::forgetNpcActivities(requests.actorsToRemove(), scripts);
    REQUIRE(world.findActor(npcId) != nullptr);
    advanced_platformer::applyWorldRequests(world, requests);

    REQUIRE(scripts.forgotten == std::vector<advanced_platformer::ActorId>{npcId});
    REQUIRE(world.findActor(npcId) == nullptr);
}

TEST_CASE("The engine fills an activity's snapshot from the world", "[npc][lua]")
{
    advanced_platformer::TileMap map = tests::TileMapBuilder({"........", "........", "##......"});
    advanced_platformer::World world({{1, "Coin", {}, 5}});
    const advanced_platformer::ActorId player = tests::addPlayer(
        world, makePlayer({56.0F, 32.0F}).onTeam(advanced_platformer::Team::Player));
    const advanced_platformer::ActorId npc = world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet({24.0F, 32.0F})
            .platforming()
            .onTeam(advanced_platformer::Team::Enemy)
            .thinking({64.0F, 1.0F})
            .running(
                tests::NpcMachineBuilder::named("test").state(
                    "acting", advanced_platformer::NpcActivity{"fixture", "act"})));
    world.setExit({.bounds = {{96.0F, 16.0F}, {16.0F, 16.0F}}});
    world.addPickup({{{{40.0F, 24.0F}, {8.0F, 8.0F}}}, {1, 1}});
    tests::RecordingNpcScripts scripts;

    advanced_platformer::updateNpcSenses(map, world, tests::FixedStepSeconds);
    advanced_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds, scripts);

    REQUIRE(scripts.calls.size() == 2);
    const advanced_platformer::NpcActivitySnapshot& snapshot = scripts.calls.back().snapshot;
    REQUIRE(snapshot.center == advanced_platformer::centerOf(actor(world, npc).body.bounds));
    REQUIRE(snapshot.exitFeet == glm::vec2{104.0F, 32.0F});
    REQUIRE(snapshot.pickups == std::vector<glm::vec2>{{44.0F, 32.0F}});
    REQUIRE(
        snapshot.targetCenter.value_or(glm::vec2{}) ==
        advanced_platformer::centerOf(actor(world, player).body.bounds));
    REQUIRE(snapshot.footing.has_value());
    REQUIRE(snapshot.footing.value_or(advanced_platformer::NpcFooting{}).left);
    REQUIRE_FALSE(snapshot.footing.value_or(advanced_platformer::NpcFooting{}).right);
    REQUIRE_FALSE(snapshot.routeStatus.has_value());
    REQUIRE(snapshot.lastKnownTargetFeet == glm::vec2{56.0F, 32.0F});
}

TEST_CASE("The snapshot says how the route search ended", "[npc][lua][navigation]")
{
    advanced_platformer::TileMap map = tests::TileMapBuilder(
        {"........", "........", "........", "........", "........", "########"});
    advanced_platformer::World world;
    const advanced_platformer::ActorId npc = world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .inCell({1, 4})
            .platforming()
            .onTeam(advanced_platformer::Team::Enemy)
            .thinking({64.0F, 1.0F})
            .running(
                tests::NpcMachineBuilder::named("test").state(
                    "acting", advanced_platformer::NpcActivity{"fixture", "act"})));
    tests::component<advanced_platformer::PlatformerMovement>(world, npc).grounded = true;
    tests::RecordingNpcScripts scripts;
    scripts.command.routeTo = advanced_platformer::feetInCell(tests::TileSize, {6, 0});

    std::optional<advanced_platformer::NavigationPathStatus> status;
    for (int tick = 0;
         tick < 300 && status != advanced_platformer::NavigationPathStatus::Unreachable;
         ++tick)
    {
        advanced_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds, scripts);
        status = scripts.calls.back().snapshot.routeStatus;
    }
    REQUIRE(status == advanced_platformer::NavigationPathStatus::Unreachable);
    REQUIRE(tests::component<advanced_platformer::PathFollower>(world, npc).path.has_value());

    scripts.command.routeTo.reset();
    scripts.command.clearRoute = true;
    advanced_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds, scripts);
    advanced_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds, scripts);
    REQUIRE_FALSE(scripts.calls.back().snapshot.routeStatus.has_value());
}

TEST_CASE("A flyer has no footing, and the last known target feet outlast the target", "[npc][lua]")
{
    advanced_platformer::TileMap map = tests::TileMapBuilder({"........", "........", "########"});
    advanced_platformer::World world;
    const advanced_platformer::ActorId npc =
        world.addActor(makeNpc({24.0F, 16.0F})
                           .running(
                               tests::NpcMachineBuilder::named("test").state(
                                   "acting", advanced_platformer::NpcActivity{"fixture", "act"})));
    tests::component<advanced_platformer::NpcBrain>(world, npc).lastKnownTargetFeet = {
        72.0F, 32.0F};
    tests::RecordingNpcScripts scripts;

    advanced_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds, scripts);

    const advanced_platformer::NpcActivitySnapshot& snapshot = scripts.calls.back().snapshot;
    REQUIRE_FALSE(snapshot.footing.has_value());
    REQUIRE_FALSE(snapshot.targetFeet.has_value());
    REQUIRE_FALSE(snapshot.targetCenter.has_value());
    REQUIRE(snapshot.lastKnownTargetFeet == glm::vec2{72.0F, 32.0F});
}

TEST_CASE("A script can turn its NPC's patrol round", "[npc][lua]")
{
    advanced_platformer::TileMap map = tests::TileMapBuilder({"........", "........", "########"});
    advanced_platformer::World world;
    const advanced_platformer::ActorId npc = world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet({24.0F, 16.0F})
            .flying(20.0F)
            .patrolling({8.0F, 16.0F}, {104.0F, 16.0F})
            .thinking({64.0F, 1.0F})
            .running(
                tests::NpcMachineBuilder::named("test").state(
                    "acting", advanced_platformer::NpcActivity{"fixture", "act"})));
    tests::RecordingNpcScripts scripts;
    scripts.command.turnPatrol = true;

    advanced_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds, scripts);
    REQUIRE(scripts.calls.back()
                .snapshot.patrol.value_or(advanced_platformer::Patrol{})
                .headingToSecond);
    REQUIRE_FALSE(tests::component<advanced_platformer::Patrol>(world, npc).headingToSecond);

    advanced_platformer::updateNpcBehaviour(map, world, tests::FixedStepSeconds, scripts);
    REQUIRE_FALSE(scripts.calls.back()
                      .snapshot.patrol.value_or(advanced_platformer::Patrol{})
                      .headingToSecond);
    REQUIRE(tests::component<advanced_platformer::Patrol>(world, npc).headingToSecond);
}
