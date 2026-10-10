#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/physics/body.hpp"
#include "support/require_near.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using advanced_platformer::Body;
    using advanced_platformer::ClimbGrip;
    using advanced_platformer::ClimbSurface;
    using advanced_platformer::InputIntentions;
    using advanced_platformer::PlatformerMovement;
    using advanced_platformer::SurfaceClimb;
    using advanced_platformer::WallHeading;

    const advanced_platformer::TileMap Wall =
        tests::TileMapBuilder({"......", "..c...", "..c...", "..c...", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
}

TEST_CASE("A climb request holds and moves along a wall", "[movement][climb]")
{
    Body body{{{48.0F, 36.0F}, {12.0F, 12.0F}}, {0.0F, 20.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbGrip = ClimbGrip::Hold;
    intentions.direction.y = -1.0F;

    advanced_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);

    REQUIRE(climb.surface == ClimbSurface::LeftWall);
    REQUIRE_NEAR(body.bounds.topLeft.y, 30.0F);
    REQUIRE_NEAR(body.velocity.y, -60.0F);
    REQUIRE_FALSE(movement.grounded);
    REQUIRE(climb.wallHeading == WallHeading::Up);

    intentions.direction = {};
    advanced_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);
    REQUIRE(climb.surface == ClimbSurface::LeftWall);
    REQUIRE_NEAR(body.bounds.topLeft.y, 30.0F);
    REQUIRE_NEAR(body.velocity.y, 0.0F);

    intentions.direction.y = 1.0F;
    advanced_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);
    REQUIRE_NEAR(body.bounds.topLeft.y, 36.0F);
    REQUIRE(climb.wallHeading == WallHeading::Down);

    intentions.direction = {};
    advanced_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);
    REQUIRE(climb.wallHeading == WallHeading::Down);
}

TEST_CASE("A wall heading follows the climb and resets off the wall", "[movement][climb]")
{
    InputIntentions still;
    InputIntentions up;
    up.direction.y = -1.0F;
    InputIntentions down;
    down.direction.y = 1.0F;

    for (const ClimbSurface wall : {ClimbSurface::LeftWall, ClimbSurface::RightWall})
    {
        REQUIRE(
            advanced_platformer::wallHeadingFor(wall, up, WallHeading::Down) == WallHeading::Up);
        REQUIRE(
            advanced_platformer::wallHeadingFor(wall, down, WallHeading::Up) == WallHeading::Down);
        REQUIRE(
            advanced_platformer::wallHeadingFor(wall, still, WallHeading::Down) ==
            WallHeading::Down);
    }
    for (const ClimbSurface offTheWall : {ClimbSurface::None, ClimbSurface::Ceiling})
    {
        REQUIRE(
            advanced_platformer::wallHeadingFor(offTheWall, down, WallHeading::Down) ==
            WallHeading::Up);
    }
}

TEST_CASE("The opposite side of a wall can also be climbed", "[movement][climb]")
{
    Body body{{{20.0F, 36.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbGrip = ClimbGrip::Hold;
    intentions.direction.y = -1.0F;

    advanced_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);

    REQUIRE(climb.surface == ClimbSurface::RightWall);
    REQUIRE_NEAR(body.bounds.topLeft.y, 30.0F);
}

TEST_CASE("A ceiling climb moves horizontally without gravity", "[movement][climb]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"......", ".ccc..", "......", "......", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    Body body{{{32.0F, 32.0F}, {12.0F, 12.0F}}, {0.0F, 20.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbGrip = ClimbGrip::Hold;
    intentions.direction.x = 1.0F;

    advanced_platformer::updateSurfaceClimbMovement(map, body, movement, climb, intentions, 0.1F);

    REQUIRE(climb.surface == ClimbSurface::Ceiling);
    REQUIRE_NEAR(body.bounds.topLeft.x, 38.0F);
    REQUIRE_NEAR(body.bounds.topLeft.y, 32.0F);
    REQUIRE_NEAR(body.velocity.x, 60.0F);
    REQUIRE_NEAR(body.velocity.y, 0.0F);
}

TEST_CASE("A wall climber can turn onto a ceiling", "[movement][climb]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"......", "..ccc.", "..c...", "..c...", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    Body body{{{48.0F, 36.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbGrip = ClimbGrip::Hold;
    intentions.direction.y = -1.0F;
    advanced_platformer::updateSurfaceClimbMovement(map, body, movement, climb, intentions, 0.1F);
    REQUIRE(climb.surface == ClimbSurface::LeftWall);
    REQUIRE_NEAR(body.bounds.topLeft.y, 32.0F);

    intentions.direction = {1.0F, 0.0F};
    advanced_platformer::updateSurfaceClimbMovement(map, body, movement, climb, intentions, 0.1F);

    REQUIRE(climb.surface == ClimbSurface::Ceiling);
    REQUIRE_NEAR(body.bounds.topLeft.x, 54.0F);
    REQUIRE_NEAR(body.bounds.topLeft.y, 32.0F);
}

TEST_CASE("Keeping the grip stays on a held wall and never grabs one", "[movement][climb]")
{
    Body body{{{48.0F, 36.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    REQUIRE(intentions.climbGrip == ClimbGrip::Keep);

    advanced_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);
    REQUIRE(climb.surface == ClimbSurface::None);

    body = Body{{{48.0F, 36.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    intentions.climbGrip = ClimbGrip::Hold;
    advanced_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);
    REQUIRE(climb.surface == ClimbSurface::LeftWall);

    intentions.climbGrip = ClimbGrip::Keep;
    advanced_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);
    REQUIRE(climb.surface == ClimbSurface::LeftWall);
    REQUIRE_NEAR(body.bounds.topLeft.y, 36.0F);
}

TEST_CASE("Releasing climb resumes ordinary falling", "[movement][climb]")
{
    Body body{{{48.0F, 36.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbGrip = ClimbGrip::Hold;
    advanced_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);
    REQUIRE(climb.surface == ClimbSurface::LeftWall);

    intentions.climbGrip = ClimbGrip::Release;
    advanced_platformer::updateSurfaceClimbMovement(Wall, body, movement, climb, intentions, 0.1F);

    REQUIRE(climb.surface == ClimbSurface::None);
    REQUIRE(body.bounds.topLeft.y > 36.0F);
    REQUIRE(body.velocity.y > 0.0F);
}

TEST_CASE("A climb request needs an adjacent surface", "[movement][climb]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"......", "......", "......", "......", "######"});
    Body body{{{48.0F, 36.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbGrip = ClimbGrip::Hold;

    advanced_platformer::updateSurfaceClimbMovement(map, body, movement, climb, intentions, 0.1F);

    REQUIRE(climb.surface == ClimbSurface::None);
    REQUIRE(body.velocity.y > 0.0F);
}

TEST_CASE("A climb request cannot attach to an unmarked solid wall", "[movement][climb]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"......", "..X...", "..X...", "..X...", "######"})
            .where('X', tests::Tile{}.blocksMovement());
    Body body{{{48.0F, 36.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbGrip = ClimbGrip::Hold;
    intentions.direction.y = -1.0F;

    advanced_platformer::updateSurfaceClimbMovement(map, body, movement, climb, intentions, 0.1F);

    REQUIRE(climb.surface == ClimbSurface::None);
    REQUIRE(body.velocity.y > 0.0F);
}

TEST_CASE("A climb request cannot attach to an unmarked solid ceiling", "[movement][climb]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"......", ".XXX..", "......", "......", "######"})
            .where('X', tests::Tile{}.blocksMovement());
    Body body{{{32.0F, 32.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbGrip = ClimbGrip::Hold;
    intentions.direction.x = 1.0F;

    advanced_platformer::updateSurfaceClimbMovement(map, body, movement, climb, intentions, 0.1F);

    REQUIRE(climb.surface == ClimbSurface::None);
    REQUIRE(body.velocity.y > 0.0F);
}

TEST_CASE("A climber off the ground grips a nearby ceiling, else a wall", "[movement][climb]")
{
    const advanced_platformer::TileMap room =
        tests::TileMapBuilder({"cccccc", "c.....", "c.....", "c.....", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    SurfaceClimb climb{{60.0F}};

    SECTION("under a ceiling")
    {
        advanced_platformer::Aabb bounds{{34.0F, 20.0F}, {12.0F, 12.0F}};
        advanced_platformer::gripNearbySurface(room, bounds, climb);
        REQUIRE(climb.surface == ClimbSurface::Ceiling);
        REQUIRE_NEAR(bounds.topLeft.y, 16.0F);
        REQUIRE_NEAR(bounds.topLeft.x, 34.0F);
    }

    SECTION("beside a wall")
    {
        advanced_platformer::Aabb bounds{{18.0F, 36.0F}, {12.0F, 12.0F}};
        advanced_platformer::gripNearbySurface(room, bounds, climb);
        REQUIRE(climb.surface == ClimbSurface::LeftWall);
        REQUIRE_NEAR(bounds.topLeft.x, 16.0F);
        REQUIRE_NEAR(bounds.topLeft.y, 36.0F);
    }

    SECTION("on the ground")
    {
        advanced_platformer::Aabb bounds{{18.0F, 52.0F}, {12.0F, 12.0F}};
        advanced_platformer::gripNearbySurface(room, bounds, climb);
        REQUIRE(climb.surface == ClimbSurface::None);
        REQUIRE_NEAR(bounds.topLeft.x, 18.0F);
    }

    SECTION("nothing in reach")
    {
        advanced_platformer::Aabb bounds{{50.0F, 36.0F}, {12.0F, 12.0F}};
        advanced_platformer::gripNearbySurface(room, bounds, climb);
        REQUIRE(climb.surface == ClimbSurface::None);
        REQUIRE_NEAR(bounds.topLeft.x, 50.0F);
        REQUIRE_NEAR(bounds.topLeft.y, 36.0F);
    }
}

TEST_CASE("Climbing ends when the surface ends", "[movement][climb]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"......", ".cc...", "......", "......", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    Body body{{{32.0F, 32.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}};
    InputIntentions intentions;
    intentions.climbGrip = ClimbGrip::Hold;
    intentions.direction.x = 1.0F;

    advanced_platformer::updateSurfaceClimbMovement(map, body, movement, climb, intentions, 0.4F);

    REQUIRE(climb.surface == ClimbSurface::None);
    REQUIRE_NEAR(body.velocity.x, 0.0F);
    REQUIRE_NEAR(body.velocity.y, 0.0F);
    advanced_platformer::updateSurfaceClimbMovement(map, body, movement, climb, intentions, 0.1F);
    REQUIRE(body.velocity.y > 0.0F);
}

TEST_CASE("Climbing requires a positive finite speed", "[movement][climb]")
{
    SurfaceClimb climb;
    climb.config.speed = 0.0F;
    REQUIRE_THROWS_AS(
        advanced_platformer::validateSurfaceClimbConfig(climb.config), std::invalid_argument);
}
