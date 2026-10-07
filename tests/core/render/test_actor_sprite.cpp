#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/render/actor_sprite.hpp"
#include "advanced_platformer/render/sprite.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"

namespace
{
    using advanced_platformer::ActorSpritePlacement;
    using advanced_platformer::ClimbSurface;
    using advanced_platformer::Facing;

    constexpr glm::vec2 SpriteSize{32.0F, 30.0F};

    advanced_platformer::Actor climber(ClimbSurface surface, Facing facing)
    {
        advanced_platformer::Actor actor = tests::ActorBuilder::sized({12.0F, 12.0F})
                                               .at({16.0F, 32.0F})
                                               .platforming()
                                               .climbing()
                                               .withSprite({0, {{0.0F, 0.0F}, SpriteSize}});
        tests::component<advanced_platformer::SurfaceClimb>(actor).surface = surface;
        actor.facing = facing;
        return actor;
    }

    glm::vec2 turn(glm::vec2 direction, float radians)
    {
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        return {
            direction.x * cosine - direction.y * sine, direction.x * sine + direction.y * cosine};
    }

    glm::vec2 feetDirection(const ActorSpritePlacement& placement)
    {
        return turn({0.0F, 1.0F}, placement.rotationRadians);
    }

    glm::vec2 headDirection(const ActorSpritePlacement& placement)
    {
        return turn({placement.flipHorizontal ? -1.0F : 1.0F, 0.0F}, placement.rotationRadians);
    }

    bool pointsAlong(glm::vec2 direction, glm::vec2 expected)
    {
        return glm::distance(direction, expected) < 0.001F;
    }

    bool near(float value, float expected)
    {
        return std::abs(value - expected) < 0.001F;
    }

    void requireVisibleIsTurnedDrawn(const ActorSpritePlacement& placement)
    {
        const glm::vec2 centre = advanced_platformer::centerOf(placement.drawn);
        const glm::vec2 half = placement.drawn.size * 0.5F;
        glm::vec2 lowest = centre;
        glm::vec2 highest = centre;
        for (const glm::vec2 corner :
             {glm::vec2{-half.x, -half.y},
              glm::vec2{half.x, -half.y},
              glm::vec2{-half.x, half.y},
              glm::vec2{half.x, half.y}})
        {
            const glm::vec2 point = centre + turn(corner, placement.rotationRadians);
            lowest = glm::min(lowest, point);
            highest = glm::max(highest, point);
        }
        REQUIRE(near(placement.visible.topLeft.x, lowest.x));
        REQUIRE(near(placement.visible.topLeft.y, lowest.y));
        REQUIRE(near(advanced_platformer::rightOf(placement.visible), highest.x));
        REQUIRE(near(advanced_platformer::bottomOf(placement.visible), highest.y));
    }
}

TEST_CASE("A sprite off every surface stands on the body's feet unturned", "[render][sprite]")
{
    advanced_platformer::Actor actor = climber(ClimbSurface::None, Facing::Left);
    const ActorSpritePlacement placement = advanced_platformer::placeActorSprite(actor);

    REQUIRE(placement.rotationRadians == 0.0F);
    REQUIRE(placement.flipHorizontal);
    const advanced_platformer::Aabb expected = advanced_platformer::spriteBounds(
        actor.body.bounds, tests::component<advanced_platformer::Sprite>(actor));
    REQUIRE(placement.drawn.topLeft == expected.topLeft);
    REQUIRE(placement.drawn.size == expected.size);
    REQUIRE(placement.visible.topLeft == expected.topLeft);
}

TEST_CASE("A climber's sprite stands its feet on the surface it holds", "[render][sprite][climb]")
{
    struct Case
    {
        ClimbSurface surface;
        glm::vec2 towardsSurface;
    };

    for (const Case& held :
         {Case{ClimbSurface::LeftWall, {-1.0F, 0.0F}},
          Case{ClimbSurface::RightWall, {1.0F, 0.0F}},
          Case{ClimbSurface::Ceiling, {0.0F, -1.0F}}})
    {
        const advanced_platformer::Actor actor = climber(held.surface, Facing::Right);
        const ActorSpritePlacement placement = advanced_platformer::placeActorSprite(actor);
        const advanced_platformer::Aabb& body = actor.body.bounds;
        const glm::vec2 bodyCentre = advanced_platformer::centerOf(body);
        const glm::vec2 visibleCentre = advanced_platformer::centerOf(placement.visible);

        REQUIRE(pointsAlong(feetDirection(placement), held.towardsSurface));
        REQUIRE(placement.drawn.size == SpriteSize);
        requireVisibleIsTurnedDrawn(placement);
        switch (held.surface)
        {
        case ClimbSurface::LeftWall:
            REQUIRE(near(placement.visible.topLeft.x, body.topLeft.x));
            REQUIRE(near(visibleCentre.y, bodyCentre.y));
            break;
        case ClimbSurface::RightWall:
            REQUIRE(near(
                advanced_platformer::rightOf(placement.visible),
                advanced_platformer::rightOf(body)));
            REQUIRE(near(visibleCentre.y, bodyCentre.y));
            break;
        case ClimbSurface::Ceiling:
            REQUIRE(near(placement.visible.topLeft.y, body.topLeft.y));
            REQUIRE(near(visibleCentre.x, bodyCentre.x));
            break;
        case ClimbSurface::None:
            break;
        }
    }
}

TEST_CASE("A climber's head leads the way it goes", "[render][sprite][climb]")
{
    REQUIRE(pointsAlong(
        headDirection(
            advanced_platformer::placeActorSprite(climber(ClimbSurface::Ceiling, Facing::Right))),
        {1.0F, 0.0F}));
    REQUIRE(pointsAlong(
        headDirection(
            advanced_platformer::placeActorSprite(climber(ClimbSurface::Ceiling, Facing::Left))),
        {-1.0F, 0.0F}));

    for (const ClimbSurface wall : {ClimbSurface::LeftWall, ClimbSurface::RightWall})
    {
        advanced_platformer::Actor actor = climber(wall, Facing::Right);
        REQUIRE(pointsAlong(
            headDirection(advanced_platformer::placeActorSprite(actor)), {0.0F, -1.0F}));

        tests::component<advanced_platformer::SurfaceClimb>(actor).wallHeading =
            advanced_platformer::WallHeading::Down;
        REQUIRE(
            pointsAlong(headDirection(advanced_platformer::placeActorSprite(actor)), {0.0F, 1.0F}));
    }
}

TEST_CASE("A centre-anchored climber's sprite turns about its body's centre", "[render][sprite]")
{
    advanced_platformer::Actor actor = climber(ClimbSurface::LeftWall, Facing::Right);
    tests::component<advanced_platformer::Sprite>(actor).anchor =
        advanced_platformer::SpriteAnchor::BodyCenter;
    const ActorSpritePlacement placement = advanced_platformer::placeActorSprite(actor);

    const glm::vec2 bodyCentre = advanced_platformer::centerOf(actor.body.bounds);
    REQUIRE(pointsAlong(advanced_platformer::centerOf(placement.drawn), bodyCentre));
    REQUIRE(pointsAlong(advanced_platformer::centerOf(placement.visible), bodyCentre));
}
