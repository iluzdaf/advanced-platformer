#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/flying_movement.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/navigation/route.hpp"
#include "advanced_platformer/navigation/navigation_path.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/navigation/platformer_connections.hpp"
#include "advanced_platformer/navigation/platformer_traversal_profile.hpp"
#include "advanced_platformer/navigation/traversal.hpp"
#include "advanced_platformer/physics/body.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "support/cell_connections.hpp"
#include "support/require_near.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"
#include "support/fixed_step.hpp"
#include "support/navigation_paths.hpp"
#include "support/route_connections.hpp"

TEST_CASE("A flying path follower produces intentions for its next step", "[navigation][follower]")
{
    advanced_platformer::PathFollower follower;
    advanced_platformer::setPath(
        follower,
        tests::floorPath(
            {0, 0},
            {{{{1, 0}}, advanced_platformer::Traversal::Fly, {}},
             {{{1, 1}}, advanced_platformer::Traversal::Fly, {}}}));
    advanced_platformer::Aabb bounds{{4.0F, 4.0F}, {8.0F, 12.0F}};
    const advanced_platformer::FlyingMovement movement;

    const advanced_platformer::InputIntentions right =
        advanced_platformer::followFlyingPath(bounds, movement, follower, tests::FixedStepSeconds);
    REQUIRE(right.direction.x == 1.0F);
    REQUIRE(right.direction.y == 0.0F);

    bounds = advanced_platformer::boxInCell(tests::TileSize, {1, 0}, bounds.size);
    const advanced_platformer::InputIntentions down =
        advanced_platformer::followFlyingPath(bounds, movement, follower, tests::FixedStepSeconds);
    REQUIRE(down.direction.x == 0.0F);
    REQUIRE(down.direction.y == 1.0F);

    bounds = advanced_platformer::boxInCell(tests::TileSize, {1, 1}, bounds.size);
    REQUIRE(
        advanced_platformer::followFlyingPath(bounds, movement, follower, tests::FixedStepSeconds)
            .direction == glm::vec2{0.0F});
    REQUIRE(advanced_platformer::pathComplete(follower));
}

TEST_CASE(
    "A flying path follower uses the exact remaining waypoint distance",
    "[navigation][follower]")
{
    advanced_platformer::PathFollower follower;
    advanced_platformer::setPath(
        follower, tests::floorPath({0, 0}, {{{{1, 0}}, advanced_platformer::Traversal::Fly, {}}}));
    advanced_platformer::Aabb bounds{{19.75F, 4.0F}, {8.0F, 12.0F}};
    const advanced_platformer::FlyingMovement movement{60.0F};

    const advanced_platformer::InputIntentions intentions =
        advanced_platformer::followFlyingPath(bounds, movement, follower, tests::FixedStepSeconds);

    REQUIRE_NEAR(intentions.direction.x, 0.25F);
    REQUIRE(intentions.direction.y == 0.0F);
    REQUIRE_FALSE(advanced_platformer::pathComplete(follower));
}

TEST_CASE(
    "A platformer path follower executes a generated jump through movement and collision",
    "[navigation][follower]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const glm::vec2 bodySize{12.0F, 12.0F};
    const advanced_platformer::PlatformerMovementConfig config;
    const std::vector<advanced_platformer::RouteConnection> connections =
        tests::connectionsFrom(
            map,
            {2, 2},
            advanced_platformer::PlatformerTraversalProfile{
                .size = bodySize, .movement = config, .stepSeconds = tests::FixedStepSeconds})
            .connections;
    const advanced_platformer::RouteConnection& jump =
        tests::connectionWith(connections, advanced_platformer::Traversal::Jump);

    advanced_platformer::PathFollower follower;
    advanced_platformer::setPath(follower, tests::floorPath({2, 2}, {jump.step}));
    advanced_platformer::Body body{
        advanced_platformer::boxInCell(tests::TileSize, {2, 2}, bodySize), {0.0F, 0.0F}};
    advanced_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};

    for (int tick = 0; tick < 180 && !advanced_platformer::pathComplete(follower); ++tick)
    {
        const advanced_platformer::InputIntentions intentions =
            advanced_platformer::followPlatformerPath(
                body, movement, follower, tests::FixedStepSeconds);
        advanced_platformer::updatePlatformerMovement(
            map, body, movement, intentions, tests::FixedStepSeconds);
    }

    REQUIRE(advanced_platformer::pathComplete(follower));
    REQUIRE(
        advanced_platformer::cellAtFeet(
            tests::TileSize, advanced_platformer::feetOf(body.bounds)) ==
        jump.step.destination.cell);
}

TEST_CASE(
    "A platformer path follower approaches and brakes without moving the body directly",
    "[navigation][follower]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const glm::vec2 bodySize{12.0F, 12.0F};
    const advanced_platformer::PlatformerMovementConfig config;
    const std::vector<advanced_platformer::RouteConnection> connections =
        tests::connectionsFrom(
            map,
            {2, 2},
            advanced_platformer::PlatformerTraversalProfile{
                .size = bodySize, .movement = config, .stepSeconds = tests::FixedStepSeconds})
            .connections;
    const advanced_platformer::RouteConnection& jump =
        tests::connectionWith(connections, advanced_platformer::Traversal::Jump);

    advanced_platformer::PathFollower follower;
    advanced_platformer::setPath(follower, tests::floorPath({2, 2}, {jump.step}));
    advanced_platformer::Body body{
        advanced_platformer::boxInCell(tests::TileSize, {2, 2}, bodySize), {80.0F, 0.0F}};
    body.bounds.topLeft.x -= 6.0F;
    advanced_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    bool preparedForJump = false;

    for (int tick = 0; tick < 240 && !advanced_platformer::pathComplete(follower); ++tick)
    {
        const glm::vec2 positionBeforeFollowing = body.bounds.topLeft;
        const glm::vec2 velocityBeforeFollowing = body.velocity;
        const advanced_platformer::InputIntentions intentions =
            advanced_platformer::followPlatformerPath(
                body, movement, follower, tests::FixedStepSeconds);
        preparedForJump = preparedForJump || follower.programElapsed == 0.0F;

        REQUIRE(body.bounds.topLeft == positionBeforeFollowing);
        REQUIRE(body.velocity == velocityBeforeFollowing);
        advanced_platformer::updatePlatformerMovement(
            map, body, movement, intentions, tests::FixedStepSeconds);
    }

    REQUIRE(preparedForJump);
    REQUIRE(advanced_platformer::pathComplete(follower));
    REQUIRE(
        advanced_platformer::cellAtFeet(
            tests::TileSize, advanced_platformer::feetOf(body.bounds)) ==
        jump.step.destination.cell);
}

TEST_CASE(
    "A platformer path follower brakes between a walk and a generated jump",
    "[navigation][follower]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const glm::vec2 bodySize{12.0F, 12.0F};
    const advanced_platformer::PlatformerMovementConfig config;
    const std::vector<advanced_platformer::RouteConnection> connections =
        tests::connectionsFrom(
            map,
            {2, 2},
            advanced_platformer::PlatformerTraversalProfile{
                .size = bodySize, .movement = config, .stepSeconds = tests::FixedStepSeconds})
            .connections;
    const advanced_platformer::RouteConnection& jump =
        tests::connectionWith(connections, advanced_platformer::Traversal::Jump);

    advanced_platformer::PathFollower follower;
    advanced_platformer::setPath(
        follower,
        tests::floorPath(
            {1, 2}, {{{{2, 2}}, advanced_platformer::Traversal::Walk, {}}, jump.step}));
    advanced_platformer::Body body{
        advanced_platformer::boxInCell(tests::TileSize, {1, 2}, bodySize), {0.0F, 0.0F}};
    advanced_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    bool brakedAfterWalking = false;

    for (int tick = 0; tick < 360 && !advanced_platformer::pathComplete(follower); ++tick)
    {
        const advanced_platformer::InputIntentions intentions =
            advanced_platformer::followPlatformerPath(
                body, movement, follower, tests::FixedStepSeconds);
        brakedAfterWalking =
            brakedAfterWalking ||
            (follower.nextStep == 0 && body.velocity.x != 0.0F && intentions.direction.x == 0.0F);
        advanced_platformer::updatePlatformerMovement(
            map, body, movement, intentions, tests::FixedStepSeconds);
    }

    REQUIRE(brakedAfterWalking);
    REQUIRE(advanced_platformer::pathComplete(follower));
    REQUIRE(
        advanced_platformer::cellAtFeet(
            tests::TileSize, advanced_platformer::feetOf(body.bounds)) ==
        jump.step.destination.cell);
}

TEST_CASE(
    "A platformer path follower drops a path whose jump lands on another row",
    "[navigation][follower]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const glm::vec2 bodySize{12.0F, 12.0F};
    const advanced_platformer::PlatformerMovementConfig config;
    const std::vector<advanced_platformer::RouteConnection> connections =
        tests::connectionsFrom(
            map,
            {2, 2},
            advanced_platformer::PlatformerTraversalProfile{
                .size = bodySize, .movement = config, .stepSeconds = tests::FixedStepSeconds})
            .connections;
    const advanced_platformer::RouteConnection& jump =
        tests::connectionWith(connections, advanced_platformer::Traversal::Jump);

    // The same jump, but its waypoint claims a row above where it really lands.
    advanced_platformer::NavigationPath path = tests::floorPath({2, 2}, {jump.step});
    path.waypoints.front().feet.y -= static_cast<float>(tests::TileSize);
    advanced_platformer::PathFollower follower;
    advanced_platformer::setPath(follower, path);
    advanced_platformer::Body body{
        advanced_platformer::boxInCell(tests::TileSize, {2, 2}, bodySize), {0.0F, 0.0F}};
    advanced_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};

    for (int tick = 0; tick < 240 && follower.path.has_value(); ++tick)
    {
        const advanced_platformer::InputIntentions intentions =
            advanced_platformer::followPlatformerPath(
                body, movement, follower, tests::FixedStepSeconds);
        advanced_platformer::updatePlatformerMovement(
            map, body, movement, intentions, tests::FixedStepSeconds);
    }

    REQUIRE_FALSE(follower.path.has_value());
    REQUIRE(movement.grounded);
}
