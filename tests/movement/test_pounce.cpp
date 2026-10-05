#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/pounce.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/physics/body.hpp"
#include "support/fixed_step.hpp"
#include "support/require_near.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using advanced_platformer::Body;
    using advanced_platformer::ClimbGrip;
    using advanced_platformer::ClimbSurface;
    using advanced_platformer::InputIntentions;
    using advanced_platformer::PlatformerMovement;
    using advanced_platformer::Pounce;
    using advanced_platformer::PounceConfig;
    using advanced_platformer::PouncePhase;
    using advanced_platformer::SurfaceClimb;

    const advanced_platformer::TileMap Room =
        tests::TileMapBuilder({"cccccc", "c....c", "c....c", "c....c", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    constexpr glm::vec2 BodySize{12.0F, 12.0F};
    constexpr PounceConfig Config{.speed = 200.0F, .lift = 120.0F, .recoveryDuration = 0.5F};

    InputIntentions pounceAt(glm::vec2 aim)
    {
        InputIntentions intentions;
        intentions.primaryAttackPressed = true;
        intentions.aimDirection = aim;
        return intentions;
    }

    Body standing()
    {
        return {{{40.0F, 52.0F}, BodySize}, {0.0F, 0.0F}};
    }

    PlatformerMovement grounded()
    {
        PlatformerMovement movement;
        movement.grounded = true;
        return movement;
    }
}

TEST_CASE(
    "A pounce from the floor leaps along the aim with at least its lift",
    "[movement][pounce]")
{
    Body body = standing();
    PlatformerMovement movement = grounded();
    Pounce pounce{.config = Config};
    glm::vec2 aim{1.0F, 0.0F};
    float expectedSpeedX = 200.0F;
    SECTION("A level aim is lifted")
    {
    }
    SECTION("A rising aim keeps its own lift")
    {
        aim = {1.0F, -1.0F};
        expectedSpeedX = 200.0F / std::sqrt(2.0F);
    }

    advanced_platformer::updatePounceMovement(
        Room, body, movement, nullptr, pounce, pounceAt(aim), true, tests::FixedStepSeconds);

    REQUIRE_NEAR(body.velocity.x, expectedSpeedX);
    REQUIRE(body.velocity.y < -100.0F);
    REQUIRE(body.bounds.topLeft.y < 52.0F);
    REQUIRE(pounce.phase == PouncePhase::Airborne);
    REQUIRE_FALSE(movement.grounded);
}

TEST_CASE("A pounce from a wall or ceiling lets go and leaps along the aim", "[movement][pounce]")
{
    Body body{{{24.0F, 16.0F}, BodySize}, {0.0F, 0.0F}};
    PlatformerMovement movement;
    SurfaceClimb climb{{60.0F}, ClimbSurface::Ceiling};
    Pounce pounce{.config = Config};
    glm::vec2 aim{0.6F, 0.8F};
    SECTION("From the ceiling, downwards")
    {
    }
    SECTION("From a wall, outwards")
    {
        body.bounds.topLeft = {16.0F, 32.0F};
        climb.surface = ClimbSurface::LeftWall;
        aim = {1.0F, 0.0F};
    }
    const glm::vec2 before = body.velocity;

    advanced_platformer::updatePounceMovement(
        Room, body, movement, &climb, pounce, pounceAt(aim), true, tests::FixedStepSeconds);

    REQUIRE_NEAR(body.velocity.x, aim.x * 200.0F);
    REQUIRE(body.velocity.y >= aim.y * 200.0F);
    REQUIRE(before == glm::vec2{0.0F, 0.0F});
    REQUIRE(climb.surface == ClimbSurface::None);
    REQUIRE(pounce.phase == PouncePhase::Airborne);

    InputIntentions holdOn;
    holdOn.climbGrip = ClimbGrip::Hold;
    holdOn.direction = {-1.0F, 0.0F};
    advanced_platformer::updatePounceMovement(
        Room, body, movement, &climb, pounce, holdOn, false, tests::FixedStepSeconds);

    REQUIRE(climb.surface == ClimbSurface::None);
    REQUIRE_NEAR(body.velocity.x, aim.x * 200.0F);
}

TEST_CASE("In flight a pounce ignores steering, then lands into a recovery", "[movement][pounce]")
{
    Body body = standing();
    PlatformerMovement movement = grounded();
    Pounce pounce{.config = Config};
    advanced_platformer::updatePounceMovement(
        Room,
        body,
        movement,
        nullptr,
        pounce,
        pounceAt({0.0F, -1.0F}),
        true,
        tests::FixedStepSeconds);

    InputIntentions steer;
    steer.direction = {-1.0F, 0.0F};
    steer.primaryAttackPressed = true;
    steer.aimDirection = {-1.0F, 0.0F};
    int ticks = 0;
    while (pounce.phase == PouncePhase::Airborne && ticks < 200)
    {
        advanced_platformer::updatePounceMovement(
            Room, body, movement, nullptr, pounce, steer, true, tests::FixedStepSeconds);
        ++ticks;
    }

    REQUIRE(pounce.phase == PouncePhase::Recovery);
    REQUIRE(pounce.phaseTimeRemaining == 0.5F);
    REQUIRE(body.velocity.x == 0.0F);
    REQUIRE(movement.grounded);
    REQUIRE(ticks > 5);

    advanced_platformer::updatePounceMovement(
        Room, body, movement, nullptr, pounce, steer, true, tests::FixedStepSeconds);
    REQUIRE(pounce.phase == PouncePhase::Recovery);

    for (int tick = 0; tick < 30; ++tick)
    {
        advanced_platformer::updatePounceMovement(
            Room, body, movement, nullptr, pounce, {}, false, tests::FixedStepSeconds);
    }
    REQUIRE(pounce.phase == PouncePhase::Ready);
}

TEST_CASE("A pounce needs to be ready, resting and aimed", "[movement][pounce]")
{
    Body body = standing();
    PlatformerMovement movement = grounded();
    Pounce pounce{.config = Config};
    InputIntentions intentions = pounceAt({1.0F, 0.0F});
    SECTION("In the air")
    {
        body.bounds.topLeft.y = 30.0F;
        movement.grounded = false;
    }
    SECTION("Without an aim")
    {
        intentions.aimDirection = {0.0F, 0.0F};
    }

    advanced_platformer::updatePounceMovement(
        Room, body, movement, nullptr, pounce, intentions, true, tests::FixedStepSeconds);

    REQUIRE(pounce.phase == PouncePhase::Ready);
    REQUIRE(body.velocity.x == 0.0F);
}

TEST_CASE("A pounce rejects invalid settings", "[movement][pounce][validation]")
{
    REQUIRE_NOTHROW(advanced_platformer::validatePounceConfig(Config));
    REQUIRE_THROWS_AS(
        advanced_platformer::validatePounceConfig({.speed = 0.0F}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        advanced_platformer::validatePounceConfig({.lift = -1.0F}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        advanced_platformer::validatePounceConfig({.range = 0.0F}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        advanced_platformer::validatePounceConfig({.recoveryDuration = -0.1F}),
        std::invalid_argument);
}
