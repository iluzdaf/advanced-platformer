#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/lifecycle.hpp"
#include "advanced_platformer/combat/attack_system.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/world/world.hpp"
#include "advanced_platformer/world/world_requests.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"

namespace
{
    advanced_platformer::Actor makeActor(glm::vec2 topLeft, advanced_platformer::Team team)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .at(topLeft)
            .platforming()
            .withHealth(3, 3)
            .onTeam(team);
    }
}

TEST_CASE("Contact damage hits an opponent once per activation, not allies", "[combat][contact]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor charger =
        makeActor({20.0F, 20.0F}, advanced_platformer::Team::Enemy);
    charger.contactDamage = advanced_platformer::ContactDamage{};
    charger.intentions.contactDamage = true;
    const auto chargerId = world.addActor(charger);
    const auto targetId =
        world.addActor(makeActor({26.0F, 20.0F}, advanced_platformer::Team::Player));
    const auto allyId = world.addActor(makeActor({26.0F, 20.0F}, advanced_platformer::Team::Enemy));
    advanced_platformer::WorldRequests requests;

    advanced_platformer::updateAttacks(world, requests, 0.1F);
    advanced_platformer::updateLifeState(world, requests, 0.0F);
    advanced_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::health(world, targetId).current == 2);
    REQUIRE(tests::health(world, allyId).current == 3);
    advanced_platformer::updateAttacks(world, requests, 0.1F);
    advanced_platformer::updateLifeState(world, requests, 0.0F);
    advanced_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::health(world, targetId).current == 2);
    REQUIRE(tests::contactDamage(world, chargerId).actorsHit.size() == 1);
    REQUIRE(tests::actor(world, chargerId).body.velocity == glm::vec2{0.0F, 0.0F});

    tests::actor(world, chargerId).intentions.contactDamage = false;
    advanced_platformer::updateAttacks(world, requests, 0.1F);
    advanced_platformer::applyWorldRequests(world, requests);
    REQUIRE_FALSE(tests::contactDamage(world, chargerId).active);
    REQUIRE(tests::contactDamage(world, chargerId).actorsHit.empty());
    REQUIRE(tests::health(world, targetId).current == 2);

    tests::actor(world, chargerId).intentions.contactDamage = true;
    advanced_platformer::updateAttacks(world, requests, 0.1F);
    advanced_platformer::updateLifeState(world, requests, 0.0F);
    advanced_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::health(world, targetId).current == 1);
}

TEST_CASE(
    "Contact damage with knockback throws the opponent away from the attacker",
    "[combat][contact]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor charger =
        makeActor({20.0F, 20.0F}, advanced_platformer::Team::Enemy);
    charger.contactDamage = advanced_platformer::ContactDamage{
        .damage = 1, .knockback = advanced_platformer::Knockback{.speed = 180.0F, .lift = 140.0F}};
    charger.intentions.contactDamage = true;
    glm::vec2 targetTopLeft{26.0F, 20.0F};
    glm::vec2 expected{180.0F, -140.0F};
    SECTION("A target to the right is thrown right")
    {
    }
    SECTION("A target to the left is thrown left")
    {
        targetTopLeft = {14.0F, 20.0F};
        expected.x = -180.0F;
    }
    SECTION("A target dead ahead is thrown the way the attacker faces")
    {
        targetTopLeft = {20.0F, 20.0F};
        charger.facing = advanced_platformer::Facing::Left;
        expected.x = -180.0F;
    }
    const auto chargerId = world.addActor(charger);
    const auto targetId =
        world.addActor(makeActor(targetTopLeft, advanced_platformer::Team::Player));
    tests::platformerMovement(tests::actor(world, targetId)).grounded = true;
    advanced_platformer::WorldRequests requests;

    advanced_platformer::updateAttacks(world, requests, 0.1F);
    advanced_platformer::updateLifeState(world, requests, 0.0F);

    REQUIRE(tests::actor(world, targetId).body.velocity == expected);
    REQUIRE_FALSE(tests::platformerMovement(tests::actor(world, targetId)).grounded);
    REQUIRE(tests::health(world, targetId).current == 2);
    REQUIRE(tests::actor(world, chargerId).body.velocity == glm::vec2{0.0F, 0.0F});
}

TEST_CASE("Contact damage needs an intention", "[combat][contact]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor attacker =
        makeActor({20.0F, 20.0F}, advanced_platformer::Team::Enemy);
    attacker.contactDamage = advanced_platformer::ContactDamage{};
    const auto attackerId = world.addActor(attacker);
    const auto targetId =
        world.addActor(makeActor({26.0F, 20.0F}, advanced_platformer::Team::Player));
    advanced_platformer::WorldRequests requests;

    advanced_platformer::updateAttacks(world, requests, 0.1F);
    advanced_platformer::applyWorldRequests(world, requests);

    REQUIRE(tests::health(world, targetId).current == 3);
    REQUIRE_FALSE(tests::contactDamage(world, attackerId).active);
}

TEST_CASE("A dying owner cannot keep contact damage active", "[combat][contact][lifecycle]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor attacker =
        makeActor({20.0F, 20.0F}, advanced_platformer::Team::Enemy);
    attacker.contactDamage = advanced_platformer::ContactDamage{};
    attacker.intentions.contactDamage = true;
    attacker.contactDamage->active = true;
    attacker.life = advanced_platformer::LifeState::Dying;
    const auto attackerId = world.addActor(attacker);
    const auto targetId =
        world.addActor(makeActor({26.0F, 20.0F}, advanced_platformer::Team::Player));
    advanced_platformer::WorldRequests requests;

    advanced_platformer::updateAttacks(world, requests, 0.1F);
    advanced_platformer::applyWorldRequests(world, requests);

    REQUIRE(tests::health(world, targetId).current == 3);
    REQUIRE_FALSE(tests::contactDamage(world, attackerId).active);
}
