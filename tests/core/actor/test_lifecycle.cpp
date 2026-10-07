#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/actor/lifecycle.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/world/world.hpp"
#include "advanced_platformer/world/world_requests.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"

namespace
{
    advanced_platformer::Actor makeActor(int health = 3)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .at({8.0F, 8.0F})
            .platforming()
            .withHealth(health, health);
    }
}

TEST_CASE("Damage is deferred until lifecycle requests are applied", "[actor][lifecycle]")
{
    advanced_platformer::World world;
    const advanced_platformer::ActorId id = world.addActor(makeActor());
    advanced_platformer::WorldRequests requests;

    requests.damage(id, 1);
    advanced_platformer::Actor& undamaged = tests::actor(world, id);
    REQUIRE(tests::component<advanced_platformer::Health>(undamaged).current == 3);

    advanced_platformer::updateLifeState(world, requests, 0.1F);

    advanced_platformer::Actor& damaged = tests::actor(world, id);
    REQUIRE(tests::component<advanced_platformer::Health>(damaged).current == 2);
    REQUIRE(damaged.life == advanced_platformer::LifeState::Alive);
    REQUIRE(requests.empty());
}

TEST_CASE("Applied damage records the current simulation time", "[actor][lifecycle]")
{
    advanced_platformer::World world;
    world.advanceSimulationTime(2.0F);
    const advanced_platformer::ActorId id = world.addActor(makeActor());
    advanced_platformer::WorldRequests requests;
    requests.damage(id, 1);

    advanced_platformer::updateLifeState(world, requests, 0.02F);

    advanced_platformer::Actor& damaged = tests::actor(world, id);
    REQUIRE(damaged.lastDamageTimeSeconds == 2.0F);
}

TEST_CASE("Damage with knockback sets the target's velocity, even when fatal", "[actor][lifecycle]")
{
    advanced_platformer::World world;
    int health = 3;
    SECTION("A survivor is thrown")
    {
    }
    SECTION("A fatal hit still throws")
    {
        health = 1;
    }
    const advanced_platformer::ActorId id = world.addActor(makeActor(health));
    tests::component<advanced_platformer::PlatformerMovement>(tests::actor(world, id)).grounded =
        true;
    advanced_platformer::WorldRequests requests;
    requests.damage(id, 1, glm::vec2{90.0F, -60.0F});

    advanced_platformer::updateLifeState(world, requests, 0.1F);

    advanced_platformer::Actor& thrown = tests::actor(world, id);
    REQUIRE(thrown.body.velocity == glm::vec2{90.0F, -60.0F});
    REQUIRE_FALSE(tests::component<advanced_platformer::PlatformerMovement>(thrown).grounded);
    REQUIRE(tests::component<advanced_platformer::Health>(thrown).current == health - 1);
    const std::vector<advanced_platformer::WorldEvent> events = world.takeEvents();
    REQUIRE(events.size() == 1);
    REQUIRE(events.front().kind == advanced_platformer::WorldEventKind::Knockback);
    REQUIRE(events.front().actor == id);
    REQUIRE(events.front().velocity == glm::vec2{90.0F, -60.0F});
}

TEST_CASE("Fatal damage begins a timed death", "[actor][lifecycle]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor actor = makeActor(1);
    actor.intentions.direction.x = 1.0F;
    const advanced_platformer::ActorId id = world.addActor(actor);
    advanced_platformer::WorldRequests requests;
    requests.damage(id, 1);

    advanced_platformer::updateLifeState(world, requests, 0.1F);

    advanced_platformer::Actor& dying = tests::actor(world, id);
    REQUIRE(tests::component<advanced_platformer::Health>(dying).current == 0);
    REQUIRE(dying.life == advanced_platformer::LifeState::Dying);
    REQUIRE(dying.deathTimeRemaining == 0.4F);
    REQUIRE(dying.lastDamageTimeSeconds == 0.0F);
    REQUIRE(dying.intentions.direction.x == 0.0F);
}

TEST_CASE("Dying actors cannot take further damage", "[actor][lifecycle]")
{
    advanced_platformer::World world;
    const advanced_platformer::ActorId id = world.addActor(makeActor(1));
    advanced_platformer::WorldRequests requests;
    requests.damage(id, 1);
    advanced_platformer::updateLifeState(world, requests, 0.1F);

    requests.damage(id, 1);
    advanced_platformer::updateLifeState(world, requests, 0.1F);

    advanced_platformer::Actor& dying = tests::actor(world, id);
    REQUIRE(tests::component<advanced_platformer::Health>(dying).current == 0);
    REQUIRE(dying.deathTimeRemaining < 0.4F);
}

TEST_CASE("An NPC is removed after its death timer", "[actor][lifecycle]")
{
    advanced_platformer::World world;
    const advanced_platformer::ActorId npc = world.addActor(makeActor(1));
    advanced_platformer::WorldRequests requests;
    requests.damage(npc, 1);
    advanced_platformer::updateLifeState(world, requests, 0.1F);

    advanced_platformer::updateLifeState(world, requests, 0.4F);

    REQUIRE(world.findActor(npc) != nullptr);
    advanced_platformer::applyWorldRequests(world, requests);
    REQUIRE(world.findActor(npc) == nullptr);
}

TEST_CASE("The player is defeated, not removed, after its death timer", "[actor][lifecycle]")
{
    advanced_platformer::World world;
    const advanced_platformer::ActorId player = world.addActor(makeActor(1));
    world.setPlayer(player, {40.0F, 48.0F});
    advanced_platformer::WorldRequests requests;
    requests.damage(player, 1);
    advanced_platformer::updateLifeState(world, requests, 0.1F);
    REQUIRE_FALSE(world.playerDefeated());

    advanced_platformer::updateLifeState(world, requests, 0.4F);
    advanced_platformer::applyWorldRequests(world, requests);

    REQUIRE(world.playerDefeated());
    REQUIRE(world.findActor(player) != nullptr);
}

TEST_CASE("Explicit removals are deferred until world requests are applied", "[world][requests]")
{
    advanced_platformer::World world;
    const advanced_platformer::ActorId id = world.addActor(makeActor());
    advanced_platformer::WorldRequests requests;
    requests.remove(id);

    REQUIRE(world.findActor(id) != nullptr);
    advanced_platformer::applyWorldRequests(world, requests);
    REQUIRE(world.findActor(id) == nullptr);
    REQUIRE(requests.empty());
}

TEST_CASE("Invalid lifecycle requests and timing are rejected", "[actor][lifecycle]")
{
    advanced_platformer::WorldRequests requests;
    REQUIRE_THROWS_AS(requests.damage({}, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(requests.damage({1}, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(requests.remove({}), std::invalid_argument);

    advanced_platformer::World world;
    REQUIRE_THROWS_AS(
        advanced_platformer::updateLifeState(world, requests, -1.0F), std::invalid_argument);
}

TEST_CASE("A knockback must be finite", "[actor][lifecycle]")
{
    advanced_platformer::World world;
    const advanced_platformer::ActorId id = world.addActor(makeActor());
    advanced_platformer::WorldRequests requests;

    REQUIRE_THROWS_AS(
        requests.damage(id, 1, glm::vec2{std::numeric_limits<float>::quiet_NaN(), 0.0F}),
        std::invalid_argument);
    REQUIRE(requests.empty());
}
