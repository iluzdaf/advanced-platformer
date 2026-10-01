#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/actor/actor_system.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_senses.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "support/require_near.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/tile_map_builder.hpp"
#include "support/add_player.hpp"
#include "support/fixed_step.hpp"

using tests::actor;
using tests::brain;

namespace
{
    tests::ActorBuilder makePlayer(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet(feet)
            .platforming()
            .onTeam(advanced_platformer::Team::Player);
    }

    tests::ActorBuilder makeNpc(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet(feet)
            .flying(20.0F)
            .onTeam(advanced_platformer::Team::Enemy)
            .thinking({64.0F, 1.0F});
    }

    // Whether an NPC with these bounds and senses sees a player with those bounds, after
    // one senses update.
    bool sees(
        const advanced_platformer::TileMap& map,
        const advanced_platformer::Aabb& observer,
        const advanced_platformer::Aabb& target,
        advanced_platformer::NpcSenses senses = {64.0F, 1.0F})
    {
        advanced_platformer::World world;
        tests::addPlayer(
            world,
            tests::ActorBuilder::sized(target.size)
                .atFeet(advanced_platformer::feetOf(target))
                .platforming()
                .onTeam(advanced_platformer::Team::Player));
        const advanced_platformer::ActorId npcId = world.addActor(
            tests::ActorBuilder::sized(observer.size)
                .atFeet(advanced_platformer::feetOf(observer))
                .flying(20.0F)
                .onTeam(advanced_platformer::Team::Enemy)
                .thinking(senses));
        advanced_platformer::updateNpcSenses(map, world, tests::FixedStepSeconds);
        return tests::perception(world, npcId).targetVisible;
    }
}

TEST_CASE("NPC sight observes distance and solid tiles", "[npc][senses]")
{
    const advanced_platformer::TileMap clear =
        tests::TileMapBuilder({".....", ".....", ".....", "#####"});
    const advanced_platformer::TileMap blocked =
        tests::TileMapBuilder({".....", "..x..", ".....", "#####"})
            .where('x', tests::Tile().blocksSight());
    const advanced_platformer::Aabb observer{{8.0F, 16.0F}, {12.0F, 12.0F}};
    const advanced_platformer::Aabb target{{56.0F, 16.0F}, {12.0F, 12.0F}};

    REQUIRE(sees(clear, observer, target));
    REQUIRE_FALSE(sees(blocked, observer, target));
    REQUIRE_FALSE(sees(clear, observer, target, {32.0F, 1.0F}));
}

TEST_CASE("A landing is heard once by a ground NPC on the same run", "[npc][senses][noise]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "########"});
    advanced_platformer::World world;
    const auto playerId = tests::addPlayer(world, makePlayer({72.0F, 47.0F}));
    const auto npcId = world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet({24.0F, 48.0F})
            .platforming()
            .onTeam(advanced_platformer::Team::Enemy)
            .thinking({64.0F, 1.0F}));
    tests::platformerMovement(actor(world, playerId)).grounded = false;
    actor(world, playerId).body.velocity.y = 40.0F;
    advanced_platformer::updateActorMovement(map, world, 0.1F);
    REQUIRE(tests::platformerMovement(actor(world, playerId)).grounded);

    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE(tests::perception(world, npcId).heardLanding);
    REQUIRE(brain(world, npcId).target == playerId);
    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE_FALSE(tests::perception(world, npcId).heardLanding);
}

TEST_CASE("Perception refreshes without clearing brain memory or machine state", "[npc][senses]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"............", "............", "...c........", "############"})
            .where('c', tests::Tile().blocksSight());
    advanced_platformer::World world;
    const auto playerId = tests::addPlayer(world, makePlayer({72.0F, 48.0F}));
    const auto npcId = world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet({24.0F, 48.0F})
            .platforming()
            .onTeam(advanced_platformer::Team::Enemy)
            .thinking({64.0F, 1.0F}));
    tests::platformerMovement(actor(world, npcId)).grounded = true;
    tests::machine(world, npcId).stateElapsed = 0.25F;
    world.emitNoise({playerId, {72.0F, 48.0F}, advanced_platformer::NoiseKind::Landing});

    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE(tests::perception(world, npcId).heardLanding);
    REQUIRE_FALSE(tests::perception(world, npcId).targetVisible);
    REQUIRE(brain(world, npcId).target == playerId);
    const auto rememberedFeet = brain(world, npcId).lastKnownTargetFeet;
    const float memoryRemaining = brain(world, npcId).targetMemoryRemaining;

    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE_FALSE(tests::perception(world, npcId).heardLanding);
    REQUIRE_FALSE(tests::perception(world, npcId).targetVisible);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE(brain(world, npcId).lastKnownTargetFeet == rememberedFeet);
    REQUIRE_NEAR(brain(world, npcId).targetMemoryRemaining, memoryRemaining - 0.1F);
    REQUIRE(
        advanced_platformer::activeNpcMachineState(tests::machine(world, npcId)).name == "idle");
    REQUIRE(tests::machine(world, npcId).stateElapsed == 0.25F);
}

TEST_CASE("A landing across a broken run is not heard", "[npc][senses][noise]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "###..###"});
    advanced_platformer::World world;
    const auto playerId = tests::addPlayer(world, makePlayer({104.0F, 48.0F}));
    const auto npcId = world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet({24.0F, 48.0F})
            .platforming()
            .onTeam(advanced_platformer::Team::Enemy)
            .thinking({128.0F, 1.0F}));
    tests::platformerMovement(actor(world, npcId)).grounded = true;
    world.emitNoise({playerId, {104.0F, 48.0F}, advanced_platformer::NoiseKind::Landing});
    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE_FALSE(tests::perception(world, npcId).heardLanding);
}

TEST_CASE("Sight-blocking cover hides whoever stands in it", "[npc][senses]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({".....", "c....", "....."}).where('c', tests::Tile().blocksSight());
    const advanced_platformer::Aabb inCover{{2.0F, 18.0F}, {12.0F, 12.0F}};
    const advanced_platformer::Aabb inOpen{{50.0F, 18.0F}, {12.0F, 12.0F}};

    REQUIRE_FALSE(sees(map, inOpen, inCover));
    REQUIRE(sees(map, inCover, inOpen));
}

TEST_CASE("Actors in one patch of sight-blocking cover see each other", "[npc][senses]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({".....", "ccc..", "....."}).where('c', tests::Tile().blocksSight());
    const advanced_platformer::Aabb first{{2.0F, 18.0F}, {12.0F, 12.0F}};
    const advanced_platformer::Aabb second{{34.0F, 18.0F}, {12.0F, 12.0F}};

    REQUIRE(sees(map, first, second));
    REQUIRE(sees(map, second, first));
}

TEST_CASE("Actors at the same position in sight-blocking cover see each other", "[npc][senses]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({".....", "c....", "....."}).where('c', tests::Tile().blocksSight());
    const advanced_platformer::Aabb inCover{{2.0F, 18.0F}, {12.0F, 12.0F}};

    REQUIRE(sees(map, inCover, inCover));
}

TEST_CASE("Actors in separate patches of sight-blocking cover are hidden", "[npc][senses]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({".....", "c.c..", "....."}).where('c', tests::Tile().blocksSight());
    const advanced_platformer::Aabb first{{2.0F, 18.0F}, {12.0F, 12.0F}};
    const advanced_platformer::Aabb second{{34.0F, 18.0F}, {12.0F, 12.0F}};

    REQUIRE_FALSE(sees(map, first, second));
    REQUIRE_FALSE(sees(map, second, first));
}

TEST_CASE("Only sight-blocking tiles block sight between actors in the open", "[npc][senses]")
{
    const advanced_platformer::TileMap covered =
        tests::TileMapBuilder({"...", ".c.", "..."}).where('c', tests::Tile().blocksSight());
    const advanced_platformer::TileMap windowed =
        tests::TileMapBuilder({"...", ".w.", "..."}).where('w', tests::Tile().blocksMovement());
    const advanced_platformer::Aabb left{{2.0F, 18.0F}, {12.0F, 12.0F}};
    const advanced_platformer::Aabb right{{34.0F, 18.0F}, {12.0F, 12.0F}};

    REQUIRE_FALSE(sees(covered, left, right));
    REQUIRE_FALSE(sees(covered, right, left));
    REQUIRE(sees(windowed, left, right));
    REQUIRE(sees(windowed, right, left));
}

TEST_CASE("NPC target memory expires and rejects a dead player", "[npc][senses]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"............", "............", "............", "############"});
    advanced_platformer::World world;
    const advanced_platformer::ActorId playerId =
        tests::addPlayer(world, makePlayer({38.0F, 28.0F}));
    const advanced_platformer::ActorId npcId = world.addActor(makeNpc({22.0F, 28.0F}));

    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE(tests::perception(world, npcId).targetVisible);

    advanced_platformer::moveFeetTo(actor(world, playerId).body.bounds, {166.0F, 28.0F});
    advanced_platformer::updateNpcSenses(map, world, 0.4F);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE_FALSE(tests::perception(world, npcId).targetVisible);
    REQUIRE_NEAR(brain(world, npcId).targetMemoryRemaining, 0.6F);

    advanced_platformer::updateNpcSenses(map, world, 0.7F);
    REQUIRE_FALSE(brain(world, npcId).target.has_value());

    advanced_platformer::moveFeetTo(actor(world, playerId).body.bounds, {38.0F, 28.0F});
    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE(tests::perception(world, npcId).targetVisible);
    actor(world, playerId).life = advanced_platformer::LifeState::Dying;
    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE_FALSE(brain(world, npcId).target.has_value());
}

TEST_CASE("An NPC remembers where it heard a hidden player shoot", "[npc][senses]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........", "..c.....", "........"})
            .where('c', tests::Tile().blocksSight());
    advanced_platformer::World world;
    const advanced_platformer::ActorId playerId =
        tests::addPlayer(world, makePlayer({40.0F, 30.0F}).shooting());
    const advanced_platformer::ActorId npcId = world.addActor(makeNpc({8.0F, 30.0F}));
    const glm::vec2 shotFeet{40.0F, 30.0F};
    const auto secondNpc = world.addActor(makeNpc({16.0F, 30.0F}));

    // A presentation stamp alone must never synthesize a hearing event.
    tests::rangedWeapon(world, playerId).lastFiredTimeSeconds = world.simulationTimeSeconds();
    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE_FALSE(brain(world, npcId).target.has_value());

    // Noise is delivered on the next sensing update, at its emission position.
    world.emitNoise(
        {playerId,
         advanced_platformer::feetOf(actor(world, playerId).body.bounds),
         advanced_platformer::NoiseKind::Shot});
    // Delivery depends on the next sensing update, not a timestamp window, and uses
    // the source's old position even after it has moved out of hearing range.
    advanced_platformer::moveFeetTo(actor(world, playerId).body.bounds, {118.0F, 30.0F});
    world.advanceSimulationTime(0.5F);
    advanced_platformer::updateNpcSenses(map, world, 0.01F);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE_FALSE(tests::perception(world, npcId).targetVisible);
    REQUIRE(brain(world, npcId).lastKnownTargetFeet == shotFeet);
    REQUIRE_NEAR(brain(world, npcId).targetMemoryRemaining, 1.0F);
    REQUIRE(brain(world, secondNpc).lastKnownTargetFeet == shotFeet);
    REQUIRE_NEAR(brain(world, secondNpc).targetMemoryRemaining, 1.0F);
    REQUIRE_FALSE(tests::perception(world, secondNpc).heardLanding);

    // The consumed shot is not heard again; memory decays without another observation.
    advanced_platformer::moveFeetTo(actor(world, playerId).body.bounds, {118.0F, 30.0F});
    world.advanceSimulationTime(0.4F);
    advanced_platformer::updateNpcSenses(map, world, 0.4F);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE(brain(world, npcId).lastKnownTargetFeet == shotFeet);
    REQUIRE_NEAR(brain(world, npcId).targetMemoryRemaining, 0.6F);
    REQUIRE_NEAR(brain(world, secondNpc).targetMemoryRemaining, 0.6F);
}

TEST_CASE("An NPC hears a shot through a wall", "[npc][senses]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({".....", "..x..", "....."}).where('x', tests::Tile().blocksSight());
    advanced_platformer::World world;
    const advanced_platformer::ActorId playerId =
        tests::addPlayer(world, makePlayer({56.0F, 30.0F}).shooting());
    const advanced_platformer::ActorId npcId = world.addActor(makeNpc({8.0F, 30.0F}));

    world.emitNoise(
        {playerId,
         advanced_platformer::feetOf(actor(world, playerId).body.bounds),
         advanced_platformer::NoiseKind::Shot});
    world.advanceSimulationTime(0.1F);
    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE_FALSE(tests::perception(world, npcId).targetVisible);
    REQUIRE(brain(world, npcId).lastKnownTargetFeet == glm::vec2{56.0F, 30.0F});
}

TEST_CASE("An NPC does not hear a shot beyond its notice distance", "[npc][senses]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "..........", ".........."});
    advanced_platformer::World world;
    const advanced_platformer::ActorId playerId =
        tests::addPlayer(world, makePlayer({104.0F, 30.0F}).shooting());
    const advanced_platformer::ActorId npcId = world.addActor(makeNpc({8.0F, 30.0F}));

    world.emitNoise(
        {playerId,
         advanced_platformer::feetOf(actor(world, playerId).body.bounds),
         advanced_platformer::NoiseKind::Shot});
    world.advanceSimulationTime(0.1F);
    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE_FALSE(brain(world, npcId).target.has_value());
}

TEST_CASE("A noise batch preserves landing facts alongside shots", "[npc][senses][noise]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "...c....", "########"})
            .where('c', tests::Tile().blocksSight());
    advanced_platformer::World world;
    const auto playerId = tests::addPlayer(world, makePlayer({72.0F, 48.0F}));
    const auto npcId = world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet({24.0F, 48.0F})
            .platforming()
            .onTeam(advanced_platformer::Team::Enemy)
            .thinking({96.0F, 1.0F}));
    tests::platformerMovement(actor(world, npcId)).grounded = true;
    world.emitNoise({playerId, {72.0F, 48.0F}, advanced_platformer::NoiseKind::Landing});
    world.emitNoise({playerId, {80.0F, 48.0F}, advanced_platformer::NoiseKind::Shot});

    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE(tests::perception(world, npcId).heardLanding);
    REQUIRE_FALSE(tests::perception(world, npcId).targetVisible);
    REQUIRE(brain(world, npcId).lastKnownTargetFeet == glm::vec2{80.0F, 48.0F});
    advanced_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE_FALSE(tests::perception(world, npcId).heardLanding);
    REQUIRE_NEAR(brain(world, npcId).targetMemoryRemaining, 0.9F);
}

TEST_CASE(
    "Sensing discards unheard noise rather than leaving it for future NPCs",
    "[npc][senses][noise]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........", "..c.....", "........"})
            .where('c', tests::Tile().blocksSight());
    advanced_platformer::World world;
    const auto playerId = tests::addPlayer(world, makePlayer({40.0F, 30.0F}));
    world.emitNoise({playerId, {40.0F, 30.0F}, advanced_platformer::NoiseKind::Shot});
    advanced_platformer::updateNpcSenses(map, world, 0.0F);
    const auto npcId = world.addActor(makeNpc({8.0F, 30.0F}));
    advanced_platformer::updateNpcSenses(map, world, 0.0F);
    REQUIRE_FALSE(brain(world, npcId).target.has_value());
}

TEST_CASE("An NPC sees into cover only from within its patch and notice distance", "[npc][senses]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"............", "...ccccccc..", "............"})
            .where('c', tests::Tile().blocksSight());
    advanced_platformer::World world;
    tests::addPlayer(world, makePlayer({56.0F, 30.0F}));
    // An NPC outside the patch cannot see in.
    const advanced_platformer::ActorId outside = world.addActor(makeNpc({8.0F, 30.0F}));
    // One in the same patch but beyond its notice distance does not notice.
    const advanced_platformer::ActorId distant = world.addActor(makeNpc({152.0F, 30.0F}));
    // One in the same patch within notice distance sees the player.
    const advanced_platformer::ActorId nearby = world.addActor(makeNpc({88.0F, 30.0F}));
    const auto sensed = [&](advanced_platformer::ActorId npc)
    { return tests::perception(world, npc).targetVisible; };

    advanced_platformer::updateNpcSenses(map, world, tests::FixedStepSeconds);
    REQUIRE_FALSE(sensed(outside));
    REQUIRE_FALSE(sensed(distant));
    REQUIRE(sensed(nearby));

    // A dying NPC no longer looks, and the sighting it recorded is cleared.
    actor(world, nearby).life = advanced_platformer::LifeState::Dying;
    advanced_platformer::updateNpcSenses(map, world, tests::FixedStepSeconds);
    REQUIRE_FALSE(sensed(nearby));
}

TEST_CASE("NPC senses reject invalid timing and sensing ranges", "[npc][validation]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"...", "...", "###"});
    const advanced_platformer::Aabb bounds{{16.0F, 16.0F}, {8.0F, 8.0F}};
    advanced_platformer::World world;

    REQUIRE_THROWS_AS(sees(map, bounds, bounds, {-1.0F, 1.0F}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        advanced_platformer::updateNpcSenses(map, world, -0.1F), std::invalid_argument);
}

TEST_CASE("A brain's living target is the remembered actor while it is alive", "[npc][senses]")
{
    advanced_platformer::World world;
    const advanced_platformer::ActorId playerId =
        tests::addPlayer(world, makePlayer({38.0F, 28.0F}));
    advanced_platformer::NpcBrain brain;
    REQUIRE(advanced_platformer::livingTarget(world, brain) == nullptr);

    brain.target = playerId;
    REQUIRE(advanced_platformer::livingTarget(world, brain) == &tests::player(world));

    tests::player(world).life = advanced_platformer::LifeState::Dying;
    REQUIRE(advanced_platformer::livingTarget(world, brain) == nullptr);

    brain.target = advanced_platformer::ActorId{999};
    REQUIRE(advanced_platformer::livingTarget(world, brain) == nullptr);
}
