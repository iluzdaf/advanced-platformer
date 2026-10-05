#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/actor/lifecycle.hpp"
#include "advanced_platformer/combat/attack_system.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/math/aabb.hpp"
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
            .withPrimary(advanced_platformer::BiteAttack{});
    }
}

TEST_CASE("A bite uses windup active and recovery phases", "[combat][bite]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor attacker =
        makeActor({10.0F, 10.0F}, advanced_platformer::Team::Enemy);
    tests::component<advanced_platformer::BiteAttack>(attacker).reach = 0.0F;
    attacker.intentions.primaryAttackPressed = true;
    const advanced_platformer::ActorId attackerId = world.addActor(attacker);
    const advanced_platformer::ActorId target =
        world.addActor(makeActor({22.0F, 10.0F}, advanced_platformer::Team::Player));
    advanced_platformer::WorldRequests requests;

    advanced_platformer::updateAttacks(world, requests, 0.1F);
    REQUIRE(tests::component<advanced_platformer::Health>(world, target).current == 3);
    REQUIRE(
        tests::component<advanced_platformer::BiteAttack>(world, attackerId).phase ==
        advanced_platformer::BitePhase::Windup);

    advanced_platformer::updateAttacks(world, requests, 0.12F);
    advanced_platformer::updateLifeState(world, requests, 0.0F);
    advanced_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::component<advanced_platformer::Health>(world, target).current == 2);
    REQUIRE(
        tests::component<advanced_platformer::BiteAttack>(world, attackerId).phase ==
        advanced_platformer::BitePhase::Active);

    advanced_platformer::updateAttacks(world, requests, 0.04F);
    advanced_platformer::updateLifeState(world, requests, 0.0F);
    advanced_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::component<advanced_platformer::Health>(world, target).current == 2);

    advanced_platformer::updateAttacks(world, requests, 0.04F);
    REQUIRE(
        tests::component<advanced_platformer::BiteAttack>(world, attackerId).phase ==
        advanced_platformer::BitePhase::Recovery);
    advanced_platformer::updateAttacks(world, requests, 0.30F);
    REQUIRE(
        tests::component<advanced_platformer::BiteAttack>(world, attackerId).phase ==
        advanced_platformer::BitePhase::Ready);
}

TEST_CASE("A ready bite is harmless and never lunges", "[combat][bite]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor attacker =
        makeActor({10.0F, 10.0F}, advanced_platformer::Team::Enemy);
    const advanced_platformer::ActorId attackerId = world.addActor(attacker);
    const advanced_platformer::ActorId target =
        world.addActor(makeActor({15.0F, 10.0F}, advanced_platformer::Team::Player));
    advanced_platformer::WorldRequests requests;

    advanced_platformer::updateAttacks(world, requests, 1.0F);
    advanced_platformer::applyWorldRequests(world, requests);

    REQUIRE(tests::component<advanced_platformer::Health>(world, target).current == 3);
    REQUIRE(tests::actor(world, attackerId).body.bounds.topLeft.x == 10.0F);
}

TEST_CASE("A committed bite completes but can miss", "[combat][bite]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor attacker =
        makeActor({10.0F, 10.0F}, advanced_platformer::Team::Enemy);
    tests::component<advanced_platformer::BiteAttack>(attacker).reach = 0.0F;
    attacker.intentions.primaryAttackPressed = true;
    const advanced_platformer::ActorId attackerId = world.addActor(attacker);
    const advanced_platformer::ActorId target =
        world.addActor(makeActor({22.0F, 10.0F}, advanced_platformer::Team::Player));
    advanced_platformer::WorldRequests requests;

    advanced_platformer::updateAttacks(world, requests, 0.0F);
    advanced_platformer::Actor& movedTarget = tests::actor(world, target);
    movedTarget.body.bounds.topLeft.x = 100.0F;
    advanced_platformer::updateAttacks(world, requests, 0.51F);
    advanced_platformer::applyWorldRequests(world, requests);

    REQUIRE(tests::component<advanced_platformer::Health>(world, target).current == 3);
    REQUIRE(
        tests::component<advanced_platformer::BiteAttack>(world, attackerId).phase ==
        advanced_platformer::BitePhase::Ready);
}

TEST_CASE("Bite hitboxes are placed in the retained facing direction", "[combat][bite]")
{
    const advanced_platformer::Aabb actor{{20.0F, 30.0F}, {12.0F, 12.0F}};
    advanced_platformer::BiteAttack bite;
    bite.hitboxSize = {10.0F, 8.0F};
    bite.reach = 4.0F;

    const advanced_platformer::Aabb right =
        advanced_platformer::biteHitbox(actor, bite, advanced_platformer::Facing::Right);
    const advanced_platformer::Aabb left =
        advanced_platformer::biteHitbox(actor, bite, advanced_platformer::Facing::Left);

    REQUIRE(right.topLeft.x == 36.0F);
    REQUIRE(left.topLeft.x == 6.0F);
    REQUIRE(right.topLeft.y == 32.0F);
    REQUIRE(left.topLeft.y == 32.0F);
}
