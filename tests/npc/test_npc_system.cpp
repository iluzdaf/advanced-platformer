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

TEST_CASE("A machine reacts to landing and blocked walking facts", "[npc][machine][movement]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"..........", ".....#....", "##########"});
    advanced_platformer::World world;
    const auto playerId = tests::addPlayer(world, makePlayer({56.0F, 32.0F}));
    actor(world, playerId).team = advanced_platformer::Team::Player;
    auto charger = tests::ActorBuilder::sized({12.0F, 12.0F})
                       .atFeet({24.0F, 32.0F})
                       .platforming()
                       .onTeam(advanced_platformer::Team::Enemy)
                       .thinking({80.0F, 1.0F})
                       .running(
                           tests::NpcMachineBuilder::named("charger")
                               .state("sleep", advanced_platformer::NpcActivity{"test", "rest"})
                               .state("charge", advanced_platformer::NpcActivity{"test", "walk"})
                               .state("stunned", advanced_platformer::NpcActivity{"test", "rest"})
                               .transition("sleep", "charge")
                               .when("heardLanding", true)
                               .when("targetOnSameSurface", true)
                               .when("targetWithinNoticeDistance", true)
                               .transition("charge", "stunned")
                               .when("movementBlocked", true))
                       .withPrimary(advanced_platformer::ContactDamage{});
    const auto npcId = world.addActor(std::move(charger));
    tests::component<advanced_platformer::PlatformerMovement>(actor(world, npcId)).grounded = true;
    tests::component<advanced_platformer::PlatformerMovement>(actor(world, playerId)).grounded =
        true;
    tests::RecordingNpcScripts scripts;
    scripts.command.intentions.direction.x = 1.0F;
    scripts.command.intentions.avoidLedges = true;
    scripts.command.intentions.primaryAttackPressed = true;
    world.recordEvent({playerId, {56.0F, 32.0F}, advanced_platformer::WorldEventKind::Landing});

    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE(tests::component<advanced_platformer::NpcPerception>(world, npcId).heardLanding);
    REQUIRE(tests::component<advanced_platformer::NpcBrain>(world, npcId).target == playerId);
    REQUIRE(
        advanced_platformer::onSameGroundRun(
            map, actor(world, npcId).body.bounds, actor(world, playerId).body.bounds));
    advanced_platformer::updateNpcBehaviour(map, world, 0.1F, scripts);
    REQUIRE(
        tests::component<advanced_platformer::NpcMachine>(world, npcId)
            .definition
            .states[tests::component<advanced_platformer::NpcMachine>(world, npcId).active]
            .name == "charge");
    REQUIRE(actor(world, npcId).intentions.direction.x == 1.0F);
    REQUIRE(actor(world, npcId).intentions.primaryAttackPressed);
    REQUIRE(actor(world, npcId).intentions.avoidLedges);

    for (int tick = 0;
         tick < 8 &&
         !tests::component<advanced_platformer::PlatformerMovement>(actor(world, npcId)).blocked;
         ++tick)
    {
        advanced_platformer::updateActorMovement(map, world, 0.1F);
    }
    REQUIRE(tests::component<advanced_platformer::PlatformerMovement>(actor(world, npcId)).blocked);
    scripts.command = {};
    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    advanced_platformer::updateNpcBehaviour(map, world, 0.1F, scripts);
    REQUIRE(
        tests::component<advanced_platformer::NpcMachine>(world, npcId)
            .definition
            .states[tests::component<advanced_platformer::NpcMachine>(world, npcId).active]
            .name == "stunned");
    REQUIRE(scripts.calls.back().snapshot.facts.movementBlocked);
    REQUIRE_FALSE(actor(world, npcId).intentions.primaryAttackPressed);
    REQUIRE(actor(world, npcId).intentions.direction.x == 0.0F);
}

TEST_CASE("NPC behaviour rejects invalid timing", "[npc][validation]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"...", "...", "###"});
    advanced_platformer::World world;
    tests::RecordingNpcScripts scripts;

    REQUIRE_THROWS_AS(
        advanced_platformer::updateNpcBehaviour(map, world, -0.1F, scripts), std::invalid_argument);
}

TEST_CASE("An NPC without a state machine cannot act", "[npc][validation]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"...", "...", "###"});
    advanced_platformer::World world;
    const advanced_platformer::ActorId npcId = world.addActor(makeNpc({8.0F, 28.0F}));
    actor(world, npcId).machine.reset();
    tests::RecordingNpcScripts scripts;

    REQUIRE_THROWS_AS(
        advanced_platformer::updateNpcBehaviour(map, world, 0.1F, scripts), std::logic_error);
    REQUIRE(scripts.calls.empty());
}
