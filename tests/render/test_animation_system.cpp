#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/render/animation.hpp"
#include "advanced_platformer/render/animation_system.hpp"
#include "advanced_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/animator.hpp"

namespace
{
    advanced_platformer::Actor makeAnimatedActor()
    {
        const advanced_platformer::Animator animator = tests::fullAnimator();
        advanced_platformer::Actor actor = tests::ActorBuilder::sized({12.0F, 12.0F})
                                               .at({0.0F, 0.0F})
                                               .platforming()
                                               .withSprite({0, {{0.0F, 0.0F}, {1.0F, 1.0F}}})
                                               .withAnimator(animator);
        tests::component<advanced_platformer::PlatformerMovement>(actor).grounded = true;
        return actor;
    }
}

TEST_CASE("A ranged actor uses Attack only during its Shoot phase", "[render][animation][system]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor rangedActor = makeAnimatedActor();
    rangedActor.team = advanced_platformer::Team::Player;
    rangedActor.primaryAttack = advanced_platformer::RangedWeapon{};
    tests::component<advanced_platformer::RangedWeapon>(rangedActor).phase =
        advanced_platformer::RangedPhase::Shoot;
    tests::component<advanced_platformer::RangedWeapon>(rangedActor).phaseTimeRemaining =
        tests::component<advanced_platformer::RangedWeapon>(rangedActor).shootDuration;
    const advanced_platformer::ActorId id = world.addActor(rangedActor);

    advanced_platformer::updateWorldAnimations(world, 0.0F);
    REQUIRE(
        tests::component<advanced_platformer::Animator>(world, id).current ==
        advanced_platformer::AnimationName::Attack);

    tests::component<advanced_platformer::RangedWeapon>(world, id).phase =
        advanced_platformer::RangedPhase::Recovery;
    tests::component<advanced_platformer::RangedWeapon>(world, id).phaseTimeRemaining =
        tests::component<advanced_platformer::RangedWeapon>(world, id).recoveryDuration;
    tests::actor(world, id).body.velocity.x = 10.0F;
    advanced_platformer::updateWorldAnimations(world, 0.0F);
    REQUIRE(
        tests::component<advanced_platformer::Animator>(world, id).current ==
        advanced_platformer::AnimationName::Move);
}

TEST_CASE("Every committed bite phase uses Attack", "[render][animation][system]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor bitingActor = makeAnimatedActor();
    bitingActor.team = advanced_platformer::Team::Enemy;
    bitingActor.primaryAttack = advanced_platformer::BiteAttack{};
    const advanced_platformer::ActorId id = world.addActor(bitingActor);

    for (const advanced_platformer::BitePhase phase : {
             advanced_platformer::BitePhase::Windup,
             advanced_platformer::BitePhase::Active,
             advanced_platformer::BitePhase::Recovery,
         })
    {
        tests::component<advanced_platformer::BiteAttack>(world, id).phase = phase;
        advanced_platformer::updateWorldAnimations(world, 0.0F);
        REQUIRE(
            tests::component<advanced_platformer::Animator>(world, id).current ==
            advanced_platformer::AnimationName::Attack);
    }
}

TEST_CASE("Death animation has priority over a shot", "[render][animation][system]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor dyingActor = makeAnimatedActor();
    dyingActor.team = advanced_platformer::Team::Player;
    dyingActor.life = advanced_platformer::LifeState::Dying;
    dyingActor.primaryAttack = advanced_platformer::RangedWeapon{};
    tests::component<advanced_platformer::RangedWeapon>(dyingActor).phase =
        advanced_platformer::RangedPhase::Shoot;
    tests::component<advanced_platformer::RangedWeapon>(dyingActor).phaseTimeRemaining =
        tests::component<advanced_platformer::RangedWeapon>(dyingActor).shootDuration;
    const advanced_platformer::ActorId id = world.addActor(dyingActor);

    advanced_platformer::updateWorldAnimations(world, 0.0F);

    REQUIRE(
        tests::component<advanced_platformer::Animator>(world, id).current ==
        advanced_platformer::AnimationName::Death);
}

TEST_CASE(
    "A climber moves or idles on its surface instead of falling",
    "[render][animation][system][climb]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor climbingActor = makeAnimatedActor();
    climbingActor.surfaceClimb = advanced_platformer::SurfaceClimb{};
    const advanced_platformer::ActorId id = world.addActor(climbingActor);
    tests::component<advanced_platformer::PlatformerMovement>(world, id).grounded = false;
    advanced_platformer::Actor& actor = tests::actor(world, id);
    const auto animate = [&world, &id]()
    {
        advanced_platformer::updateWorldAnimations(world, 0.0F);
        return tests::component<advanced_platformer::Animator>(world, id).current;
    };

    tests::component<advanced_platformer::SurfaceClimb>(actor).surface =
        advanced_platformer::ClimbSurface::LeftWall;
    actor.body.velocity = {0.0F, 60.0F};
    REQUIRE(animate() == advanced_platformer::AnimationName::Move);
    actor.body.velocity = {0.0F, 0.0F};
    REQUIRE(animate() == advanced_platformer::AnimationName::Idle);

    tests::component<advanced_platformer::SurfaceClimb>(actor).surface =
        advanced_platformer::ClimbSurface::Ceiling;
    actor.body.velocity = {60.0F, 0.0F};
    REQUIRE(animate() == advanced_platformer::AnimationName::Move);

    tests::component<advanced_platformer::SurfaceClimb>(actor).surface =
        advanced_platformer::ClimbSurface::None;
    actor.body.velocity = {0.0F, 60.0F};
    REQUIRE(animate() == advanced_platformer::AnimationName::Fall);
}

TEST_CASE("World animation rejects a negative delta time", "[render][animation][system]")
{
    advanced_platformer::World world;

    REQUIRE_THROWS_AS(
        advanced_platformer::updateWorldAnimations(world, -0.1F), std::invalid_argument);
}
