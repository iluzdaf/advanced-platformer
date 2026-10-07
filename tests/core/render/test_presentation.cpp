#include <catch2/catch_test_macros.hpp>

#include <optional>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/render/animation.hpp"
#include "advanced_platformer/render/presentation.hpp"
#include "advanced_platformer/render/presentation_scripts.hpp"
#include "advanced_platformer/render/camera.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/tile_map_builder.hpp"
#include "support/animator.hpp"
#include "support/presentation_scripts.hpp"

TEST_CASE(
    "Presenting the world animates actors and fades cover in one call",
    "[render][presentation]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"......", "...cc.", "......"})
                                                 .where('c', tests::Tile().blocksSight());
    advanced_platformer::World world;
    tests::addPlayer(
        world, tests::ActorBuilder::sized({12.0F, 12.0F}).inCell({0, 1}).platforming());
    const advanced_platformer::ActorId npc = world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .inCell({4, 1})
            .flying(0.0F)
            .withSprite({0, {{0.0F, 0.0F}, {1.0F, 1.0F}}})
            .withAnimator(tests::fullAnimator()));

    tests::NoEffects noEffects;
    advanced_platformer::CameraShake shake;
    advanced_platformer::updateWorldPresentation(map, world, 0.0F, noEffects, shake);

    REQUIRE(
        tests::component<advanced_platformer::Animator>(world, npc).current ==
        advanced_platformer::AnimationName::Idle);
    REQUIRE(tests::actor(world, npc).screenVisibility == 0.0F);
}

TEST_CASE(
    "Presenting the world hands its events to the scripts and applies their effects",
    "[render][presentation]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"....", "....", "...."});
    advanced_platformer::World world;
    const advanced_platformer::ActorId player = tests::addPlayer(
        world, tests::ActorBuilder::sized({12.0F, 12.0F}).inCell({0, 1}).platforming());
    const advanced_platformer::ActorId npc =
        world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F}).inCell({2, 1}).platforming());
    tests::EffectsFrom shakeOnPlayerKnockback(
        [](const advanced_platformer::WorldEvent& event, bool isPlayer)
        {
            advanced_platformer::PresentationEffects effects;
            if (isPlayer && event.kind == advanced_platformer::WorldEventKind::Knockback)
            {
                effects.shake = advanced_platformer::CameraShakeEffect{0.5F, 3.0F};
            }
            return effects;
        });
    advanced_platformer::CameraShake shake;
    world.recordEvent({npc, {40.0F, 32.0F}, advanced_platformer::WorldEventKind::Knockback});
    world.recordEvent({player, {8.0F, 32.0F}, advanced_platformer::WorldEventKind::Landing});

    advanced_platformer::updateWorldPresentation(map, world, 0.01F, shakeOnPlayerKnockback, shake);
    REQUIRE(shakeOnPlayerKnockback.calls == 2);
    REQUIRE_FALSE(shake.active());

    world.recordEvent({player, {8.0F, 32.0F}, advanced_platformer::WorldEventKind::Knockback});
    advanced_platformer::updateWorldPresentation(map, world, 0.01F, shakeOnPlayerKnockback, shake);
    REQUIRE(shakeOnPlayerKnockback.calls == 3);
    REQUIRE(shake.active());
    REQUIRE(world.takeEvents().empty());
}
