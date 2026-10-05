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
            .onTeam(team)
            .withPrimary(advanced_platformer::ContactDamage{});
    }
}

TEST_CASE("Contact damage hits an opponent once per activation, not allies", "[combat][contact]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor charger =
        makeActor({20.0F, 20.0F}, advanced_platformer::Team::Enemy);
    charger.intentions.primaryAttackPressed = true;
    const auto chargerId = world.addActor(charger);
    const auto targetId =
        world.addActor(makeActor({26.0F, 20.0F}, advanced_platformer::Team::Player));
    const auto allyId = world.addActor(makeActor({26.0F, 20.0F}, advanced_platformer::Team::Enemy));
    advanced_platformer::WorldRequests requests;

    advanced_platformer::updateAttacks(world, requests, 0.1F);
    advanced_platformer::updateLifeState(world, requests, 0.0F);
    advanced_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::component<advanced_platformer::Health>(world, targetId).current == 2);
    REQUIRE(tests::component<advanced_platformer::Health>(world, allyId).current == 3);
    advanced_platformer::updateAttacks(world, requests, 0.1F);
    advanced_platformer::updateLifeState(world, requests, 0.0F);
    advanced_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::component<advanced_platformer::Health>(world, targetId).current == 2);
    REQUIRE(
        tests::component<advanced_platformer::ContactDamage>(world, chargerId).actorsHit.size() ==
        1);
    REQUIRE(tests::actor(world, chargerId).body.velocity == glm::vec2{0.0F, 0.0F});

    tests::actor(world, chargerId).intentions.primaryAttackPressed = false;
    advanced_platformer::updateAttacks(world, requests, 0.1F);
    advanced_platformer::applyWorldRequests(world, requests);
    REQUIRE_FALSE(tests::component<advanced_platformer::ContactDamage>(world, chargerId).active);
    REQUIRE(
        tests::component<advanced_platformer::ContactDamage>(world, chargerId).actorsHit.empty());
    REQUIRE(tests::component<advanced_platformer::Health>(world, targetId).current == 2);

    tests::actor(world, chargerId).intentions.primaryAttackPressed = true;
    advanced_platformer::updateAttacks(world, requests, 0.1F);
    advanced_platformer::updateLifeState(world, requests, 0.0F);
    advanced_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::component<advanced_platformer::Health>(world, targetId).current == 1);
}

TEST_CASE(
    "Contact damage with knockback throws the opponent away from the attacker",
    "[combat][contact]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor charger =
        makeActor({20.0F, 20.0F}, advanced_platformer::Team::Enemy);
    tests::component<advanced_platformer::ContactDamage>(charger).knockback =
        advanced_platformer::Knockback{.speed = 180.0F, .lift = 140.0F};
    charger.intentions.primaryAttackPressed = true;
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
    tests::component<advanced_platformer::PlatformerMovement>(tests::actor(world, targetId))
        .grounded = true;
    advanced_platformer::WorldRequests requests;

    advanced_platformer::updateAttacks(world, requests, 0.1F);
    advanced_platformer::updateLifeState(world, requests, 0.0F);

    REQUIRE(tests::actor(world, targetId).body.velocity == expected);
    REQUIRE_FALSE(
        tests::component<advanced_platformer::PlatformerMovement>(tests::actor(world, targetId))
            .grounded);
    REQUIRE(tests::component<advanced_platformer::Health>(world, targetId).current == 2);
    REQUIRE(tests::actor(world, chargerId).body.velocity == glm::vec2{0.0F, 0.0F});
}

TEST_CASE("Contact damage needs an intention", "[combat][contact]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor attacker =
        makeActor({20.0F, 20.0F}, advanced_platformer::Team::Enemy);
    const auto attackerId = world.addActor(attacker);
    const auto targetId =
        world.addActor(makeActor({26.0F, 20.0F}, advanced_platformer::Team::Player));
    advanced_platformer::WorldRequests requests;

    advanced_platformer::updateAttacks(world, requests, 0.1F);
    advanced_platformer::applyWorldRequests(world, requests);

    REQUIRE(tests::component<advanced_platformer::Health>(world, targetId).current == 3);
    REQUIRE_FALSE(tests::component<advanced_platformer::ContactDamage>(world, attackerId).active);
}

TEST_CASE("A dying owner cannot keep contact damage active", "[combat][contact][lifecycle]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor attacker =
        makeActor({20.0F, 20.0F}, advanced_platformer::Team::Enemy);
    attacker.intentions.primaryAttackPressed = true;
    tests::component<advanced_platformer::ContactDamage>(attacker).active = true;
    attacker.life = advanced_platformer::LifeState::Dying;
    const auto attackerId = world.addActor(attacker);
    const auto targetId =
        world.addActor(makeActor({26.0F, 20.0F}, advanced_platformer::Team::Player));
    advanced_platformer::WorldRequests requests;

    advanced_platformer::updateAttacks(world, requests, 0.1F);
    advanced_platformer::applyWorldRequests(world, requests);

    REQUIRE(tests::component<advanced_platformer::Health>(world, targetId).current == 3);
    REQUIRE_FALSE(tests::component<advanced_platformer::ContactDamage>(world, attackerId).active);
}
