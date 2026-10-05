#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <utility>

#include <glm/vec2.hpp>

#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/movement/flying_movement.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_attacks.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/npc_machine_builder.hpp"
#include "support/actor_components.hpp"

namespace
{
    template <typename Builder, typename = void> struct CanPlatform : std::false_type
    {
    };

    template <typename Builder>
    struct CanPlatform<Builder, std::void_t<decltype(std::declval<Builder>().platforming())>>
        : std::true_type
    {
    };
}

static_assert(!CanPlatform<tests::ActorBuilder::Sized>::value);
static_assert(CanPlatform<tests::ActorBuilder::Placed>::value);
static_assert(!std::is_convertible_v<tests::ActorBuilder::Sized, advanced_platformer::Actor>);
static_assert(!std::is_convertible_v<tests::ActorBuilder::Placed, advanced_platformer::Actor>);
static_assert(std::is_convertible_v<tests::ActorBuilder, advanced_platformer::Actor>);

TEST_CASE(
    "The actor builder places a body by its corner, its feet, or its cell",
    "[support][actor-builder]")
{
    const advanced_platformer::Actor byCorner =
        tests::ActorBuilder::sized({12.0F, 20.0F}).at({8.0F, 4.0F}).platforming();
    const advanced_platformer::Actor byFeet =
        tests::ActorBuilder::sized({12.0F, 20.0F}).atFeet({24.0F, 32.0F}).platforming();
    const advanced_platformer::Actor byCell =
        tests::ActorBuilder::sized({12.0F, 20.0F}).inCell({1, 1}).platforming();

    REQUIRE(byCorner.body.bounds.topLeft == glm::vec2{8.0F, 4.0F});
    REQUIRE(byCorner.body.bounds.size == glm::vec2{12.0F, 20.0F});
    REQUIRE(advanced_platformer::feetOf(byFeet.body.bounds) == glm::vec2{24.0F, 32.0F});
    REQUIRE(byFeet.body.bounds.size == glm::vec2{12.0F, 20.0F});
    REQUIRE(advanced_platformer::feetOf(byCell.body.bounds) == glm::vec2{24.0F, 32.0F});
    REQUIRE(byCell.body.bounds.size == glm::vec2{12.0F, 20.0F});
}

TEST_CASE("The actor builder gives exactly one movement component", "[support][actor-builder]")
{
    advanced_platformer::Actor platformer =
        tests::ActorBuilder::sized({12.0F, 20.0F}).at({0.0F, 0.0F}).platforming();
    advanced_platformer::Actor flyer =
        tests::ActorBuilder::sized({12.0F, 20.0F}).at({0.0F, 0.0F}).flying(40.0F);

    REQUIRE(platformer.platformerMovement.has_value());
    REQUIRE_FALSE(platformer.flyingMovement.has_value());
    REQUIRE(tests::component<advanced_platformer::FlyingMovement>(flyer).speed == 40.0F);
    REQUIRE_FALSE(flyer.platformerMovement.has_value());
}

TEST_CASE("An NPC from the actor builder is one World accepts", "[support][actor-builder]")
{
    advanced_platformer::World world;
    const advanced_platformer::ActorId id = world.addActor(
        tests::ActorBuilder::sized({12.0F, 20.0F})
            .atFeet({24.0F, 32.0F})
            .platforming()
            .onTeam(advanced_platformer::Team::Enemy)
            .thinking({64.0F, 2.0F})
            .patrolling({8.0F, 32.0F}, {56.0F, 32.0F})
            .withPrimary(advanced_platformer::BiteAttack{}));

    advanced_platformer::Actor& npc = tests::actor(world, id);
    REQUIRE(npc.team == advanced_platformer::Team::Enemy);
    REQUIRE(
        advanced_platformer::activeNpcMachineState(
            tests::component<advanced_platformer::NpcMachine>(npc))
            .name == "idle");
    REQUIRE_FALSE(tests::component<advanced_platformer::NpcPerception>(npc).targetVisible);
    REQUIRE_FALSE(tests::component<advanced_platformer::NpcPerception>(npc).heardLanding);
    REQUIRE(tests::component<advanced_platformer::NpcSenses>(npc).noticeDistance == 64.0F);
    REQUIRE(tests::component<advanced_platformer::NpcSenses>(npc).targetMemoryDuration == 2.0F);
    REQUIRE_FALSE(tests::component<advanced_platformer::PathFollower>(npc).path.has_value());
    REQUIRE(tests::component<advanced_platformer::Patrol>(npc).firstFeet == glm::vec2{8.0F, 32.0F});
    REQUIRE(
        tests::component<advanced_platformer::Patrol>(npc).secondFeet == glm::vec2{56.0F, 32.0F});
    REQUIRE(advanced_platformer::findAttack<advanced_platformer::BiteAttack>(npc) != nullptr);
}

TEST_CASE("A thinking actor from the builder can run a machine", "[support][actor-builder]")
{
    advanced_platformer::World world;
    const advanced_platformer::ActorId id = world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .at({8.0F, 8.0F})
            .platforming()
            .thinking({})
            .running(
                tests::NpcMachineBuilder::named("test").state(
                    "rest", tests::testActivity("idle"))));
    REQUIRE(
        advanced_platformer::activeNpcMachineState(
            tests::actor(world, id).machine.value_or(advanced_platformer::NpcMachine{}))
            .name == "rest");
}
