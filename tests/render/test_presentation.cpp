#include <catch2/catch_test_macros.hpp>

#include <optional>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/render/animation.hpp"
#include "advanced_platformer/render/presentation.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/tile_map_builder.hpp"
#include "support/animator.hpp"

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

    advanced_platformer::updateWorldPresentation(map, world, 0.0F);

    REQUIRE(
        tests::component<advanced_platformer::Animator>(world, npc).current ==
        advanced_platformer::AnimationName::Idle);
    REQUIRE(tests::actor(world, npc).screenVisibility == 0.0F);
}
