#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/pounce.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/render/animation.hpp"
#include "advanced_platformer/render/animation_system.hpp"
#include "advanced_platformer/render/sprite.hpp"
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

    float frameOf(advanced_platformer::World& world, advanced_platformer::ActorId id)
    {
        advanced_platformer::updateWorldAnimations(world, 0.0F);
        return tests::component<advanced_platformer::Sprite>(world, id).region.position.x;
    }
}

TEST_CASE("Each attack shows its own clip while it animates", "[render][animation][system]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor actor = makeAnimatedActor();
    actor.team = advanced_platformer::Team::Enemy;
    SECTION("A shot during its Shoot phase")
    {
        actor.primaryAttack = advanced_platformer::RangedWeapon{};
        const advanced_platformer::ActorId id = world.addActor(actor);
        auto& weapon = tests::component<advanced_platformer::RangedWeapon>(world, id);

        weapon.phase = advanced_platformer::RangedPhase::Shoot;
        REQUIRE(frameOf(world, id) == 4.0F);
        weapon.phase = advanced_platformer::RangedPhase::Recovery;
        tests::actor(world, id).body.velocity.x = 10.0F;
        REQUIRE(frameOf(world, id) == 1.0F);
    }
    SECTION("A bite in every committed phase")
    {
        actor.primaryAttack = advanced_platformer::BiteAttack{};
        const advanced_platformer::ActorId id = world.addActor(actor);
        for (const advanced_platformer::BitePhase phase : {
                 advanced_platformer::BitePhase::Windup,
                 advanced_platformer::BitePhase::Active,
                 advanced_platformer::BitePhase::Recovery,
             })
        {
            tests::component<advanced_platformer::BiteAttack>(world, id).phase = phase;
            REQUIRE(frameOf(world, id) == 5.0F);
        }
    }
    SECTION("A pounce while airborne")
    {
        actor.primaryAttack = advanced_platformer::Pounce{};
        const advanced_platformer::ActorId id = world.addActor(actor);
        auto& pounce = tests::component<advanced_platformer::Pounce>(world, id);

        pounce.phase = advanced_platformer::PouncePhase::Airborne;
        REQUIRE(frameOf(world, id) == 6.0F);
        pounce.phase = advanced_platformer::PouncePhase::Recovery;
        REQUIRE(frameOf(world, id) == 0.0F);
    }
}

TEST_CASE("Death outranks an attack and a pounce outranks a bite", "[render][animation][system]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor actor = makeAnimatedActor();
    actor.team = advanced_platformer::Team::Enemy;
    actor.primaryAttack = advanced_platformer::BiteAttack{};
    actor.secondaryAttack = advanced_platformer::Pounce{};
    const advanced_platformer::ActorId id = world.addActor(actor);
    tests::component<advanced_platformer::BiteAttack>(world, id).phase =
        advanced_platformer::BitePhase::Active;
    REQUIRE(frameOf(world, id) == 5.0F);

    tests::component<advanced_platformer::Pounce>(world, id).phase =
        advanced_platformer::PouncePhase::Airborne;
    REQUIRE(frameOf(world, id) == 6.0F);

    tests::actor(world, id).life = advanced_platformer::LifeState::Dying;
    REQUIRE(frameOf(world, id) == 7.0F);
}

TEST_CASE("A state without a clip falls through to the next one", "[render][animation][system]")
{
    advanced_platformer::World world;
    advanced_platformer::Actor actor = makeAnimatedActor();
    actor.team = advanced_platformer::Team::Enemy;
    actor.primaryAttack = advanced_platformer::BiteAttack{};
    advanced_platformer::AnimationSet& set =
        tests::component<advanced_platformer::Animator>(actor).animationSet;
    set.clips = {
        tests::clip(advanced_platformer::AnimationName::Idle, 0.0F),
        tests::clip(advanced_platformer::AnimationName::Move, 1.0F)};
    const advanced_platformer::ActorId id = world.addActor(actor);
    tests::component<advanced_platformer::BiteAttack>(world, id).phase =
        advanced_platformer::BitePhase::Active;

    REQUIRE(frameOf(world, id) == 0.0F);
    tests::actor(world, id).body.velocity.x = 10.0F;
    REQUIRE(frameOf(world, id) == 1.0F);
    tests::actor(world, id).life = advanced_platformer::LifeState::Dying;
    REQUIRE(frameOf(world, id) == 1.0F);
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

    tests::component<advanced_platformer::SurfaceClimb>(actor).surface =
        advanced_platformer::ClimbSurface::LeftWall;
    actor.body.velocity = {0.0F, 60.0F};
    REQUIRE(frameOf(world, id) == 1.0F);
    actor.body.velocity = {0.0F, 0.0F};
    REQUIRE(frameOf(world, id) == 0.0F);

    tests::component<advanced_platformer::SurfaceClimb>(actor).surface =
        advanced_platformer::ClimbSurface::Ceiling;
    actor.body.velocity = {60.0F, 0.0F};
    REQUIRE(frameOf(world, id) == 1.0F);

    tests::component<advanced_platformer::SurfaceClimb>(actor).surface =
        advanced_platformer::ClimbSurface::None;
    actor.body.velocity = {0.0F, 60.0F};
    REQUIRE(frameOf(world, id) == 3.0F);
}

TEST_CASE("World animation rejects a negative delta time", "[render][animation][system]")
{
    advanced_platformer::World world;

    REQUIRE_THROWS_AS(
        advanced_platformer::updateWorldAnimations(world, -0.1F), std::invalid_argument);
}
