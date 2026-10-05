#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/input/input_program.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/route.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/navigation/platformer_cells.hpp"
#include "advanced_platformer/navigation/platformer_connections.hpp"
#include "advanced_platformer/navigation/platformer_traversal_profile.hpp"
#include "advanced_platformer/navigation/traversal.hpp"
#include "advanced_platformer/physics/body.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "support/cell_connections.hpp"
#include "support/fixed_step.hpp"
#include "support/navigation_paths.hpp"
#include "support/route_connections.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    using advanced_platformer::Cell;
    using advanced_platformer::PlatformerTraversalProfile;
    using advanced_platformer::RouteConnection;
    using advanced_platformer::Traversal;

    constexpr glm::vec2 SmallBody{12.0F, 12.0F};

    bool hasConnection(
        const std::vector<RouteConnection>& connections,
        Cell destination,
        Traversal traversal)
    {
        return std::ranges::any_of(
            connections,
            [destination, traversal](const RouteConnection& connection)
            {
                return connection.step.destination.cell == destination &&
                       connection.step.traversal == traversal;
            });
    }
}

TEST_CASE("Walk connections reach every cell on the floor", "[navigation][platformer]")
{
    const PlatformerTraversalProfile profile{
        .size = SmallBody, .stepSeconds = tests::FixedStepSeconds};
    const advanced_platformer::TileMap floor = tests::TileMapBuilder({"......", "######"});
    const std::vector<RouteConnection> walks =
        tests::connectionsFrom(floor, {1, 0}, profile).connections;
    REQUIRE(
        std::ranges::count_if(
            walks,
            [](const RouteConnection& connection)
            { return connection.step.traversal == Traversal::Walk; }) == 5);
    REQUIRE(hasConnection(walks, {0, 0}, Traversal::Walk));
    REQUIRE(hasConnection(walks, {5, 0}, Traversal::Walk));
}

TEST_CASE("A direct walk costs less than stopping along the way", "[navigation][platformer]")
{
    const PlatformerTraversalProfile profile{
        .size = SmallBody, .stepSeconds = tests::FixedStepSeconds};
    const advanced_platformer::TileMap floor = tests::TileMapBuilder({"......", "######"});
    const std::vector<RouteConnection> walks =
        tests::connectionsFrom(floor, {1, 0}, profile).connections;
    const RouteConnection& direct = tests::connectionWith(walks, {3, 0}, Traversal::Walk);
    const RouteConnection& first = tests::connectionWith(walks, {2, 0}, Traversal::Walk);
    const std::vector<RouteConnection> onward =
        tests::connectionsFrom(floor, {2, 0}, profile).connections;
    const RouteConnection& second = tests::connectionWith(onward, {3, 0}, Traversal::Walk);
    REQUIRE(direct.cost < first.cost + second.cost);
}

TEST_CASE("A fall from a ledge records inputs", "[navigation][platformer]")
{
    const advanced_platformer::TileMap ledge =
        tests::TileMapBuilder({"........", "###.....", "........", "........", "########"});
    const PlatformerTraversalProfile profile{
        .size = SmallBody, .stepSeconds = tests::FixedStepSeconds};
    const std::vector<RouteConnection> offTheEdge =
        tests::connectionsFrom(ledge, {2, 0}, profile).connections;
    const RouteConnection& fall = tests::connectionWith(offTheEdge, Traversal::Fall);
    REQUIRE(fall.step.destination.cell.y > 0);
    REQUIRE_FALSE(fall.step.inputs.empty());
}

TEST_CASE("A jump reaches the platform above and records inputs", "[navigation][platformer]")
{
    const advanced_platformer::TileMap platform =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const PlatformerTraversalProfile profile{
        .size = SmallBody, .stepSeconds = tests::FixedStepSeconds};
    const std::vector<RouteConnection> beside =
        tests::connectionsFrom(platform, {2, 2}, profile).connections;
    const RouteConnection& jump = tests::jumpUpFrom(beside, 2);
    REQUIRE_FALSE(jump.step.inputs.empty());
}

TEST_CASE("A recorded jump replays to the landing it promised", "[navigation][platformer]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const advanced_platformer::PlatformerMovementConfig config;
    const PlatformerTraversalProfile profile{
        .size = SmallBody, .movement = config, .stepSeconds = tests::FixedStepSeconds};
    const std::vector<RouteConnection> connections =
        tests::connectionsFrom(map, {2, 2}, profile).connections;
    const RouteConnection& jump = tests::connectionWith(connections, Traversal::Jump);

    advanced_platformer::Body body{
        advanced_platformer::boxInCell(tests::TileSize, {2, 2}, SmallBody), {0.0F, 0.0F}};
    advanced_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    const float duration = advanced_platformer::durationOf(jump.step.inputs);
    const long tickCount = std::lround(duration / tests::FixedStepSeconds);
    for (int tick = 0; tick < tickCount; ++tick)
    {
        const float elapsed = static_cast<float>(tick) * tests::FixedStepSeconds;
        const advanced_platformer::InputIntentions intentions =
            advanced_platformer::replayInput(jump.step.inputs, elapsed);
        advanced_platformer::updatePlatformerMovement(
            map, body, movement, intentions, tests::FixedStepSeconds);
    }

    REQUIRE(movement.grounded);
    REQUIRE(
        advanced_platformer::cellAtFeet(
            tests::TileSize, advanced_platformer::feetOf(body.bounds)) ==
        jump.step.destination.cell);
}

TEST_CASE("Failed airborne attempts still count their simulated ticks", "[navigation][platformer]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"##.##", "#####"});
    const PlatformerTraversalProfile profile{
        .size = SmallBody, .stepSeconds = tests::FixedStepSeconds};

    const tests::CellConnections built = tests::connectionsFrom(map, {2, 0}, profile);

    REQUIRE(built.connections.empty());
    REQUIRE(built.simulatedTicks > 0);
}

TEST_CASE("A walk connection costs the ticks its follower takes", "[navigation][platformer]")
{
    const advanced_platformer::PlatformerMovementConfig config;
    const PlatformerTraversalProfile profile{
        .size = SmallBody, .movement = config, .stepSeconds = tests::FixedStepSeconds};
    const advanced_platformer::TileMap walkMap = tests::TileMapBuilder({"....", "####"});
    const std::vector<RouteConnection> walkConnections =
        tests::connectionsFrom(walkMap, {1, 0}, profile).connections;
    const RouteConnection& walk = tests::connectionWith(walkConnections, Traversal::Walk);
    advanced_platformer::PathFollower follower;
    advanced_platformer::setPath(follower, tests::floorPath({1, 0}, {walk.step}));
    advanced_platformer::Body body{
        advanced_platformer::boxInCell(tests::TileSize, {1, 0}, SmallBody), {0.0F, 0.0F}};
    advanced_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
    int walkTicks = 0;
    while (walkTicks < 120 && !advanced_platformer::pathComplete(follower))
    {
        const advanced_platformer::InputIntentions intentions =
            advanced_platformer::followPlatformerPath(
                body, movement, follower, tests::FixedStepSeconds);
        if (!advanced_platformer::pathComplete(follower))
        {
            advanced_platformer::updatePlatformerMovement(
                walkMap, body, movement, intentions, tests::FixedStepSeconds);
            ++walkTicks;
        }
    }
    REQUIRE(advanced_platformer::pathComplete(follower));
    REQUIRE(walk.cost == walkTicks);
}

TEST_CASE("Airborne connection costs use the recorded program's ticks", "[navigation][platformer]")
{
    const PlatformerTraversalProfile profile{
        .size = SmallBody, .stepSeconds = tests::FixedStepSeconds};

    SECTION("jump")
    {
        const advanced_platformer::TileMap map =
            tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
        const std::vector<RouteConnection> connections =
            tests::connectionsFrom(map, {2, 2}, profile).connections;
        const RouteConnection& jump = tests::connectionWith(connections, Traversal::Jump);
        REQUIRE_THAT(
            advanced_platformer::durationOf(jump.step.inputs),
            Catch::Matchers::WithinAbs(
                static_cast<float>(jump.cost) * profile.stepSeconds, 0.001F));
    }

    SECTION("fall")
    {
        const advanced_platformer::TileMap map =
            tests::TileMapBuilder({"........", "###.....", "........", "........", "########"});
        const std::vector<RouteConnection> connections =
            tests::connectionsFrom(map, {2, 0}, profile).connections;
        const RouteConnection& fall = tests::connectionWith(connections, Traversal::Fall);
        REQUIRE_THAT(
            advanced_platformer::durationOf(fall.step.inputs),
            Catch::Matchers::WithinAbs(
                static_cast<float>(fall.cost) * profile.stepSeconds, 0.001F));
    }
}

TEST_CASE("Platformer connections reject an invalid step", "[navigation][platformer][validation]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"...", "###"});
    const advanced_platformer::PlatformerMovementConfig movement;
    for (const float step :
         {0.0F, -tests::FixedStepSeconds, std::numeric_limits<float>::infinity()})
    {
        REQUIRE_THROWS_AS(
            tests::connectionsFrom(
                map,
                {0, 0},
                PlatformerTraversalProfile{
                    .size = SmallBody, .movement = movement, .stepSeconds = step}),
            std::invalid_argument);
    }
}

TEST_CASE(
    "A climber's cell holds the climbs leaving each surface",
    "[navigation][platformer][climb]")
{
    using advanced_platformer::ClimbSurface;
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"......", ".c....", ".c....", ".c....", ".c....", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    const PlatformerTraversalProfile walker{
        .size = SmallBody, .stepSeconds = tests::FixedStepSeconds};
    const PlatformerTraversalProfile climber{
        .size = SmallBody,
        .stepSeconds = tests::FixedStepSeconds,
        .climb = advanced_platformer::SurfaceClimbConfig{.speed = 60.0F}};
    const Cell besideWall{2, 4};

    const tests::CellConnections walking = tests::connectionsFrom(map, besideWall, walker);
    const tests::CellConnections climbing = tests::connectionsFrom(map, besideWall, climber);
    const auto climbs = [&climbing](ClimbSurface from, Cell cell, ClimbSurface surface)
    {
        return std::ranges::any_of(
            climbing.connections,
            [from, cell, surface](const RouteConnection& connection)
            {
                return connection.step.traversal == Traversal::Climb &&
                       connection.sourceSurface == from &&
                       connection.step.destination.cell == cell &&
                       connection.step.destination.surface == surface &&
                       !connection.step.inputs.empty() && connection.cost > 0;
            });
    };

    REQUIRE_FALSE(hasConnection(walking.connections, besideWall, Traversal::Climb));
    REQUIRE(climbs(ClimbSurface::None, besideWall, ClimbSurface::LeftWall));
    REQUIRE(climbs(ClimbSurface::LeftWall, besideWall, ClimbSurface::None));
    REQUIRE(climbs(ClimbSurface::LeftWall, {2, 3}, ClimbSurface::LeftWall));
    REQUIRE_FALSE(climbs(ClimbSurface::None, besideWall, ClimbSurface::RightWall));
    REQUIRE_FALSE(climbs(ClimbSurface::LeftWall, {2, 5}, ClimbSurface::LeftWall));
    REQUIRE(hasConnection(climbing.connections, {3, 4}, Traversal::Walk));
    REQUIRE(climbing.simulatedTicks > walking.simulatedTicks);
    REQUIRE(advanced_platformer::contains(climbing.footprint, {1, 3}));

    const tests::CellConnections upTheWall = tests::connectionsFrom(map, {2, 2}, climber);
    REQUIRE_FALSE(upTheWall.connections.empty());
    REQUIRE(
        std::ranges::all_of(
            upTheWall.connections,
            [](const RouteConnection& connection)
            {
                return (connection.step.traversal == Traversal::Climb ||
                        connection.step.traversal == Traversal::Fall) &&
                       connection.sourceSurface == ClimbSurface::LeftWall;
            }));
    REQUIRE(hasConnection(upTheWall.connections, {2, 4}, Traversal::Fall));
    REQUIRE(tests::connectionsFrom(map, {2, 2}, walker).connections.empty());
}

TEST_CASE(
    "A tile-sized climber changes between wall and ceiling at the same position",
    "[navigation][platformer][climb]")
{
    using advanced_platformer::ClimbSurface;
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({".ccc.", "c...c", "c...c", "ccccc"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    const auto side = static_cast<float>(tests::TileSize);
    const advanced_platformer::SurfaceClimbConfig climbConfig{.speed = 60.0F};
    const PlatformerTraversalProfile climber{
        .size = {side, side}, .stepSeconds = tests::FixedStepSeconds, .climb = climbConfig};

    for (const Cell cell : {Cell{1, 1}, Cell{3, 1}})
    {
        const ClimbSurface wall = cell.x == 1 ? ClimbSurface::LeftWall : ClimbSurface::RightWall;
        const std::vector<RouteConnection> connections =
            tests::connectionsFrom(map, cell, climber).connections;
        for (const ClimbSurface from : {wall, ClimbSurface::Ceiling})
        {
            const ClimbSurface to = from == wall ? ClimbSurface::Ceiling : wall;
            CAPTURE(cell.x, from, to);
            const auto connection = std::ranges::find_if(
                connections,
                [cell, from, to](const RouteConnection& candidate)
                {
                    return candidate.sourceSurface == from &&
                           candidate.step.destination.cell == cell &&
                           candidate.step.destination.surface == to;
                });
            REQUIRE(connection != connections.end());

            const advanced_platformer::Aabb bounds =
                advanced_platformer::boundsAtSurface(tests::TileSize, {cell, from}, climber.size);
            advanced_platformer::Body body{bounds, {0.0F, 0.0F}};
            advanced_platformer::PlatformerMovement movement{climber.movement};
            advanced_platformer::SurfaceClimb climb{climbConfig, from};
            const advanced_platformer::InputProgram& inputs = connection->step.inputs;
            const long ticks =
                std::lround(advanced_platformer::durationOf(inputs) / climber.stepSeconds);
            for (long tick = 0; tick < ticks; ++tick)
            {
                advanced_platformer::updateSurfaceClimbMovement(
                    map,
                    body,
                    movement,
                    climb,
                    advanced_platformer::replayInput(
                        inputs, static_cast<float>(tick) * climber.stepSeconds),
                    climber.stepSeconds);
            }
            REQUIRE(climb.surface == to);
            REQUIRE(body.bounds.topLeft == bounds.topLeft);
        }
    }
}

TEST_CASE(
    "A climber lets go of a wall or ceiling and falls to the floor below",
    "[navigation][platformer][climb]")
{
    using advanced_platformer::ClimbSurface;
    const advanced_platformer::TileMap room =
        tests::TileMapBuilder({"cccccc", "c....c", "c....c", "c....c", "c....c", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    const advanced_platformer::SurfaceClimbConfig climbConfig{.speed = 60.0F};
    const PlatformerTraversalProfile climber{
        .size = SmallBody, .stepSeconds = tests::FixedStepSeconds, .climb = climbConfig};
    const auto fallsFrom = [&room, &climber](Cell cell, ClimbSurface surface)
    {
        std::vector<RouteConnection> falls;
        for (const RouteConnection& connection :
             tests::connectionsFrom(room, cell, climber).connections)
        {
            if (connection.step.traversal == Traversal::Fall && connection.sourceSurface == surface)
            {
                falls.push_back(connection);
            }
        }
        return falls;
    };

    const std::vector<RouteConnection> fromCeiling = fallsFrom({2, 1}, ClimbSurface::Ceiling);
    REQUIRE(fromCeiling.size() == 1);
    REQUIRE(fromCeiling.front().step.destination == advanced_platformer::RouteLocation{{2, 4}});
    REQUIRE(fromCeiling.front().cost > 0);
    REQUIRE_FALSE(fromCeiling.front().step.inputs.empty());
    REQUIRE(
        fromCeiling.front().step.inputs.front().intentions.climbGrip ==
        advanced_platformer::ClimbGrip::Release);

    const std::vector<RouteConnection> fromWall = fallsFrom({1, 2}, ClimbSurface::LeftWall);
    REQUIRE(fromWall.size() == 1);
    REQUIRE(fromWall.front().step.destination == advanced_platformer::RouteLocation{{1, 4}});

    REQUIRE(fallsFrom({1, 4}, ClimbSurface::LeftWall).empty());
    REQUIRE(fallsFrom({2, 1}, ClimbSurface::None).empty());

    advanced_platformer::Body body{
        advanced_platformer::boundsAtSurface(
            tests::TileSize, {{2, 1}, ClimbSurface::Ceiling}, climber.size),
        {0.0F, 0.0F}};
    advanced_platformer::PlatformerMovement movement{climber.movement};
    advanced_platformer::SurfaceClimb climb{climbConfig, ClimbSurface::Ceiling};
    const advanced_platformer::InputProgram& inputs = fromCeiling.front().step.inputs;
    const long ticks = std::lround(advanced_platformer::durationOf(inputs) / climber.stepSeconds);
    for (long tick = 0; tick < ticks; ++tick)
    {
        advanced_platformer::updateSurfaceClimbMovement(
            room,
            body,
            movement,
            climb,
            advanced_platformer::replayInput(
                inputs, static_cast<float>(tick) * climber.stepSeconds),
            climber.stepSeconds);
    }
    REQUIRE(climb.surface == ClimbSurface::None);
    REQUIRE(movement.grounded);
    REQUIRE(
        advanced_platformer::cellAtFeet(
            tests::TileSize, advanced_platformer::feetOf(body.bounds)) == Cell{2, 4});
}

TEST_CASE("A climber cannot hold an unmarked wall", "[navigation][platformer][climb]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"......", ".#....", ".#....", ".#....", ".#....", "######"});
    const PlatformerTraversalProfile climber{
        .size = SmallBody,
        .stepSeconds = tests::FixedStepSeconds,
        .climb = advanced_platformer::SurfaceClimbConfig{.speed = 60.0F}};

    const std::vector<RouteConnection> connections =
        tests::connectionsFrom(map, {2, 4}, climber).connections;
    REQUIRE(hasConnection(connections, {3, 4}, Traversal::Walk));
    REQUIRE(
        std::ranges::none_of(
            connections,
            [](const RouteConnection& connection)
            { return connection.step.traversal == Traversal::Climb; }));
}

TEST_CASE(
    "A climb may reach a wall location that extends above the open map top",
    "[navigation][platformer][climb]")
{
    using advanced_platformer::ClimbSurface;
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({".c..", ".c..", "####"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    const PlatformerTraversalProfile tallClimber{
        .size = {12.0F, 40.0F},
        .stepSeconds = tests::FixedStepSeconds,
        .climb = advanced_platformer::SurfaceClimbConfig{.speed = 60.0F}};

    const std::vector<RouteConnection> connections =
        tests::connectionsFrom(map, {2, 1}, tallClimber).connections;
    REQUIRE(
        std::ranges::any_of(
            connections,
            [](const RouteConnection& connection)
            {
                return connection.step.traversal == Traversal::Climb &&
                       connection.sourceSurface == ClimbSurface::LeftWall &&
                       connection.step.destination.cell == Cell{2, 0} &&
                       connection.step.destination.surface == ClimbSurface::LeftWall;
            }));
}
