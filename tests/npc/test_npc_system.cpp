#include <catch2/catch_test_macros.hpp>

#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/actor/actor_system.hpp"
#include "advanced_platformer/combat/attack_system.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/navigation/navigation_fill.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/npc/npc_system.hpp"
#include "advanced_platformer/npc/npc_senses.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "advanced_platformer/world/world_requests.hpp"
#include "support/tile_map_builder.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/npc_machine_builder.hpp"
#include "support/recording_npc_scripts.hpp"

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

TEST_CASE("NPC behaviour rejects invalid timing", "[npc][validation]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"...", "...", "###"});
    advanced_platformer::World world;

    REQUIRE_THROWS_AS(
        advanced_platformer::updateNpcBehaviour(map, world, -0.1F), std::invalid_argument);
}

TEST_CASE("A chasing NPC searches for a lost target, then patrols again", "[npc][fsm]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"............", "............", "............", "############"});
    advanced_platformer::World world;
    const advanced_platformer::ActorId npcId =
        world.addActor(makeNpc({22.0F, 28.0F}).patrolling({24.0F, 32.0F}, {72.0F, 32.0F}));
    brain(world, npcId).state = advanced_platformer::NpcState::Chase;
    brain(world, npcId).lastKnownTargetFeet = {22.0F, 28.0F};
    tests::senses(actor(world, npcId)).searchDuration = 0.25F;

    advanced_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == advanced_platformer::NpcState::Search);

    advanced_platformer::updateNpcBehaviour(map, world, 0.1F);
    advanced_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == advanced_platformer::NpcState::Search);

    advanced_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == advanced_platformer::NpcState::Patrol);
}

TEST_CASE("A chasing NPC that does not search patrols again at once", "[npc][fsm]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"............", "............", "............", "############"});
    advanced_platformer::World world;
    const advanced_platformer::ActorId npcId =
        world.addActor(makeNpc({22.0F, 28.0F}).patrolling({24.0F, 32.0F}, {72.0F, 32.0F}));
    brain(world, npcId).state = advanced_platformer::NpcState::Chase;
    tests::senses(actor(world, npcId)).searchDuration = 0.0F;

    advanced_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == advanced_platformer::NpcState::Patrol);
}

TEST_CASE("A KeepDistance NPC shoots once its target is at its standoff", "[npc][fsm]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "########"});
    advanced_platformer::World world;
    const advanced_platformer::ActorId playerId =
        tests::addPlayer(world, makePlayer({24.0F, 32.0F}));
    const advanced_platformer::ActorId npcId =
        world.addActor(makeNpc({88.0F, 32.0F}).onTeam(advanced_platformer::Team::Enemy).shooting());
    brain(world, npcId).tactic = advanced_platformer::NpcTactic::KeepDistance;
    tests::senses(actor(world, npcId)).standoffDistance = 48.0F;
    brain(world, npcId).state = advanced_platformer::NpcState::Retreat;
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = {24.0F, 32.0F};
    tests::perception(world, npcId).targetVisible = true;

    advanced_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(brain(world, npcId).state == advanced_platformer::NpcState::Shoot);
    REQUIRE(actor(world, npcId).intentions.direction == glm::vec2{0.0F, 0.0F});
    REQUIRE(actor(world, npcId).intentions.primaryAttackPressed);
}

TEST_CASE("An NPC with a machine takes its activity from the machine, not its tactic", "[npc][fsm]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "########"});
    advanced_platformer::World world;
    const advanced_platformer::ActorId playerId =
        tests::addPlayer(world, makePlayer({70.0F, 28.0F}));
    const advanced_platformer::ActorId npcId =
        world.addActor(makeNpc({24.0F, 32.0F})
                           .running(tests::NpcMachineBuilder::named("test")
                                        .state("nap", advanced_platformer::NpcState::Watch)
                                        .state("hunt", advanced_platformer::NpcState::Chase)
                                        .transition("nap", "hunt")
                                        .when("targetKnown", true)));
    brain(world, npcId).tactic = advanced_platformer::NpcTactic::KeepDistance;

    advanced_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(advanced_platformer::activeNpcMachineState(machine(world, npcId)).name == "nap");
    REQUIRE(actor(world, npcId).intentions.aimDirection.x != 0.0F);

    // This target is close enough for the KeepDistance tactic to retreat, but the machine
    // enters Chase and moves towards it instead.
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = {70.0F, 28.0F};
    tests::perception(world, npcId).targetVisible = true;
    advanced_platformer::updateNpcBehaviour(map, world, 0.1F);
    REQUIRE(
        advanced_platformer::activeNpcMachineState(
            actor(world, npcId).machine.value_or(advanced_platformer::NpcMachine{}))
            .name == "hunt");
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);
}

TEST_CASE("A machine-controlled NPC does not copy its activity into the enum brain", "[npc][fsm]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "########"});
    advanced_platformer::World world;
    const advanced_platformer::ActorId npcId =
        world.addActor(makeNpc({24.0F, 32.0F})
                           .running(tests::NpcMachineBuilder::named("test").state(
                               "watch", advanced_platformer::NpcState::Watch)));
    brain(world, npcId).lastKnownTargetFeet = {70.0F, 32.0F};

    advanced_platformer::updateNpcBehaviour(map, world, 0.1F);

    REQUIRE(actor(world, npcId).intentions.aimDirection.x > 0.0F);
    REQUIRE(brain(world, npcId).state == advanced_platformer::NpcState::Idle);
}

TEST_CASE("An NPC without a bite continues chasing at close range", "[npc][fsm]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    advanced_platformer::World world;
    const advanced_platformer::ActorId playerId =
        tests::addPlayer(world, makePlayer({38.0F, 28.0F}));
    const advanced_platformer::ActorId npcId = world.addActor(makeNpc({22.0F, 28.0F}));
    brain(world, npcId).target = playerId;
    brain(world, npcId).lastKnownTargetFeet = {38.0F, 28.0F};
    tests::perception(world, npcId).targetVisible = true;

    advanced_platformer::updateNpcBehaviour(map, world, 0.1F);

    REQUIRE(brain(world, npcId).state == advanced_platformer::NpcState::Chase);
    REQUIRE_FALSE(actor(world, npcId).intentions.primaryAttackPressed);
    REQUIRE(actor(world, npcId).intentions.direction.x > 0.0F);
}

TEST_CASE("A machine reacts to landing and blocked walking facts", "[npc][machine][movement]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"..........", ".....#....", "##########"});
    advanced_platformer::World world;
    const auto playerId = tests::addPlayer(world, makePlayer({56.0F, 32.0F}));
    actor(world, playerId).team = advanced_platformer::Team::Player;
    auto charger =
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet({24.0F, 32.0F})
            .platforming()
            .onTeam(advanced_platformer::Team::Enemy)
            .thinking({80.0F, 1.0F})
            .running(tests::NpcMachineBuilder::named("charger")
                         .state("sleep", advanced_platformer::LuaNpcActivity{"test", "rest"})
                         .state("charge", advanced_platformer::LuaNpcActivity{"test", "walk"})
                         .state("stunned", advanced_platformer::LuaNpcActivity{"test", "rest"})
                         .transition("sleep", "charge")
                         .when("heardLanding", true)
                         .when("targetOnSameRun", true)
                         .when("targetWithinNoticeDistance", true)
                         .transition("charge", "stunned")
                         .when("movementBlocked", true))
            .withContactDamage();
    const auto npcId = world.addActor(std::move(charger));
    tests::platformerMovement(actor(world, npcId)).grounded = true;
    tests::platformerMovement(actor(world, playerId)).grounded = true;
    tests::RecordingNpcScripts scripts;
    scripts.command.intentions.direction.x = 1.0F;
    scripts.command.intentions.avoidLedges = true;
    scripts.command.intentions.contactDamage = true;
    world.emitNoise({playerId, {56.0F, 32.0F}, advanced_platformer::NoiseKind::Landing});

    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE(tests::perception(world, npcId).heardLanding);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE(advanced_platformer::onSameGroundRun(
        map, actor(world, npcId).body.bounds, actor(world, playerId).body.bounds));
    advanced_platformer::updateNpcBehaviour(map, world, 0.1F, &scripts);
    REQUIRE(machine(world, npcId).definition.states[machine(world, npcId).active].name == "charge");
    REQUIRE(actor(world, npcId).intentions.direction.x == 1.0F);
    REQUIRE(actor(world, npcId).intentions.contactDamage);
    REQUIRE(actor(world, npcId).intentions.avoidLedges);

    for (int tick = 0; tick < 8 && !tests::platformerMovement(actor(world, npcId)).blocked; ++tick)
    {
        advanced_platformer::updateActorMovement(map, world, 0.1F);
    }
    REQUIRE(tests::platformerMovement(actor(world, npcId)).blocked);
    scripts.command = {};
    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    advanced_platformer::updateNpcBehaviour(map, world, 0.1F, &scripts);
    REQUIRE(
        machine(world, npcId).definition.states[machine(world, npcId).active].name == "stunned");
    REQUIRE(scripts.calls.back().snapshot.facts.movementBlocked);
    REQUIRE_FALSE(actor(world, npcId).intentions.contactDamage);
    REQUIRE(actor(world, npcId).intentions.direction.x == 0.0F);
}
