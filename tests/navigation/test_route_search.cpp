#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "advanced_platformer/input/input_program.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/route.hpp"
#include "advanced_platformer/navigation/route_search.hpp"
#include "advanced_platformer/navigation/traversal.hpp"

namespace
{
    using advanced_platformer::Cell;
    using advanced_platformer::ClimbSurface;
    using advanced_platformer::ConnectionFunction;
    using advanced_platformer::CostFunction;
    using advanced_platformer::endOf;
    using advanced_platformer::findLowestCostRoute;
    using advanced_platformer::GridSize;
    using advanced_platformer::HeuristicFunction;
    using advanced_platformer::InputProgram;
    using advanced_platformer::Route;
    using advanced_platformer::RouteConnection;
    using advanced_platformer::RouteLocation;
    using advanced_platformer::RouteSearchResult;
    using advanced_platformer::Traversal;

    constexpr GridSize TestGrid{8, 8};

    RouteLocation floorOf(int x, int y)
    {
        return {{x, y}};
    }

    RouteConnection connectionTo(
        RouteLocation destination,
        Traversal traversal,
        int cost,
        InputProgram inputs = {})
    {
        return {{destination, traversal, std::move(inputs)}, cost};
    }

    ConnectionFunction connectionsFrom(
        std::vector<std::pair<Cell, std::vector<RouteConnection>>> table)
    {
        return [table = std::move(table)](RouteLocation location)
        {
            for (const auto& [cell, connections] : table)
            {
                if (cell == location.cell)
                {
                    return std::span<const RouteConnection>(connections);
                }
            }
            return std::span<const RouteConnection>{};
        };
    }

    ConnectionFunction lineUpTo(int lastColumn)
    {
        return [lastColumn, next = std::vector<RouteConnection>{}](RouteLocation location) mutable
        {
            next.clear();
            const Cell cell = location.cell;
            if (cell.x < lastColumn)
            {
                next.push_back(connectionTo(floorOf(cell.x + 1, cell.y), Traversal::Fly, 1));
            }
            return std::span<const RouteConnection>(next);
        };
    }

    std::span<const RouteConnection> noConnections(RouteLocation)
    {
        return {};
    }

    int zeroHeuristic(Cell, Cell)
    {
        return 0;
    }

    int gridSteps(Cell cell, Cell goal)
    {
        return std::abs(cell.x - goal.x) + std::abs(cell.y - goal.y);
    }

    Route routeOf(const RouteSearchResult& result)
    {
        return result.route.value_or(Route{});
    }
}

TEST_CASE(
    "A search that cannot move or starts in the goal cell returns a route with no steps",
    "[navigation][search]")
{
    const RouteSearchResult unreachable =
        findLowestCostRoute(floorOf(0, 0), {1, 0}, TestGrid, noConnections, zeroHeuristic);
    REQUIRE(unreachable.route.has_value());
    REQUIRE(routeOf(unreachable).steps.empty());

    const RouteSearchResult alreadyThere =
        findLowestCostRoute(floorOf(2, 3), {2, 3}, TestGrid, noConnections, zeroHeuristic);
    REQUIRE(alreadyThere.route.has_value());
    const Route route = routeOf(alreadyThere);
    REQUIRE(route.start.cell == Cell{2, 3});
    REQUIRE(route.steps.empty());
}

TEST_CASE(
    "A search treats a cell's floor, walls and ceiling as separate places",
    "[navigation][search]")
{
    const RouteLocation wall{{0, 0}, ClimbSurface::LeftWall};
    const RouteLocation ceiling{{0, 0}, ClimbSurface::Ceiling};
    const std::vector<RouteConnection> fromFloor{
        connectionTo(ceiling, Traversal::Climb, 5), connectionTo(wall, Traversal::Climb, 1)};
    const std::vector<RouteConnection> fromWall{connectionTo(ceiling, Traversal::Climb, 1)};
    const std::vector<RouteConnection> fromCeiling{
        connectionTo({{1, 0}, ClimbSurface::Ceiling}, Traversal::Climb, 1)};
    const ConnectionFunction connections = [&](RouteLocation location)
    {
        switch (location.surface)
        {
        case ClimbSurface::None:
            return std::span<const RouteConnection>(fromFloor);
        case ClimbSurface::LeftWall:
            return std::span<const RouteConnection>(fromWall);
        case ClimbSurface::Ceiling:
            return std::span<const RouteConnection>(fromCeiling);
        case ClimbSurface::RightWall:
            break;
        }
        return std::span<const RouteConnection>{};
    };

    const RouteSearchResult result =
        findLowestCostRoute(floorOf(0, 0), {1, 0}, {2, 1}, connections, zeroHeuristic);
    REQUIRE(result.route.has_value());
    const Route route = routeOf(result);
    REQUIRE(route.steps.size() == 3);
    REQUIRE(route.steps[0].destination == wall);
    REQUIRE(route.steps[1].destination == ceiling);
    REQUIRE(route.steps[2].destination.cell == Cell{1, 0});
}

TEST_CASE(
    "A search picks the cheapest route, however many cells each connection crosses",
    "[navigation][search]")
{
    const ConnectionFunction connections = connectionsFrom(
        {{{0, 0},
          {connectionTo(floorOf(2, 0), Traversal::Jump, 8),
           connectionTo(floorOf(1, 0), Traversal::Walk, 1)}},
         {{1, 0}, {connectionTo(floorOf(2, 0), Traversal::Walk, 1)}}});
    const RouteSearchResult walks =
        findLowestCostRoute(floorOf(0, 0), {2, 0}, TestGrid, connections, zeroHeuristic);
    REQUIRE(walks.route.has_value());
    const Route walkRoute = routeOf(walks);
    REQUIRE(walkRoute.steps.size() == 2);
    REQUIRE(walkRoute.steps.front().traversal == Traversal::Walk);

    const ConnectionFunction leaping =
        connectionsFrom({{{0, 0}, {connectionTo(floorOf(4, 0), Traversal::Jump, 2)}}});
    const RouteSearchResult leap =
        findLowestCostRoute(floorOf(0, 0), {4, 0}, TestGrid, leaping, zeroHeuristic);
    REQUIRE(leap.route.has_value());
    const Route leapRoute = routeOf(leap);
    REQUIRE(leapRoute.steps.size() == 1);
    REQUIRE(leapRoute.steps.front().destination == floorOf(4, 0));
}

TEST_CASE(
    "A search that cannot reach the goal cell returns a route as close to it as possible",
    "[navigation][search]")
{
    const ConnectionFunction line = lineUpTo(2);

    const RouteSearchResult outOfReach =
        findLowestCostRoute(floorOf(0, 0), {5, 0}, TestGrid, line, gridSteps);
    REQUIRE(outOfReach.route.has_value());
    REQUIRE(endOf(routeOf(outOfReach)) == floorOf(2, 0));
    REQUIRE(routeOf(outOfReach).steps.size() == 2);

    const RouteSearchResult inReach =
        findLowestCostRoute(floorOf(0, 0), {2, 0}, TestGrid, line, gridSteps);
    REQUIRE(inReach.route.has_value());
    REQUIRE(endOf(routeOf(inReach)) == floorOf(2, 0));
}

TEST_CASE(
    "A search chooses by the connections' costs, not by how many steps a route takes",
    "[navigation][search]")
{
    const auto withJumpCosting = [](int jumpCost)
    {
        return connectionsFrom(
            {{{0, 0},
              {connectionTo(floorOf(2, 0), Traversal::Jump, jumpCost, {{0.5F, {}}}),
               connectionTo(floorOf(1, 0), Traversal::Walk, 1)}},
             {{1, 0}, {connectionTo(floorOf(2, 0), Traversal::Walk, 1)}}});
    };

    const RouteSearchResult aroundJump =
        findLowestCostRoute(floorOf(0, 0), {2, 0}, TestGrid, withJumpCosting(6), zeroHeuristic);
    REQUIRE(aroundJump.route.has_value());
    const Route walkRoute = routeOf(aroundJump);
    REQUIRE(walkRoute.steps.size() == 2);
    REQUIRE(walkRoute.steps.front().traversal == Traversal::Walk);

    const RouteSearchResult overJump =
        findLowestCostRoute(floorOf(0, 0), {2, 0}, TestGrid, withJumpCosting(1), zeroHeuristic);
    REQUIRE(overJump.route.has_value());
    const Route jumpRoute = routeOf(overJump);
    REQUIRE(jumpRoute.steps.size() == 1);
    REQUIRE(jumpRoute.steps.front().traversal == Traversal::Jump);
    REQUIRE(jumpRoute.steps.front().inputs.size() == 1);
}

TEST_CASE("A search can charge connections differently from their own cost", "[navigation][search]")
{
    const ConnectionFunction connections = connectionsFrom(
        {{{0, 0},
          {connectionTo(floorOf(2, 0), Traversal::Jump, 1),
           connectionTo(floorOf(1, 0), Traversal::Walk, 1)}},
         {{1, 0}, {connectionTo(floorOf(2, 0), Traversal::Walk, 1)}}});
    const CostFunction penalisedJumps = [](const RouteConnection& connection)
    { return connection.cost + (connection.step.traversal == Traversal::Jump ? 5 : 0); };

    const RouteSearchResult result = findLowestCostRoute(
        floorOf(0, 0), {2, 0}, TestGrid, connections, zeroHeuristic, penalisedJumps);
    REQUIRE(result.route.has_value());
    REQUIRE(routeOf(result).steps.size() == 2);
    REQUIRE(routeOf(result).steps.front().traversal == Traversal::Walk);
    REQUIRE(connections({{0, 0}}).value_or(std::span<const RouteConnection>{}).front().cost == 1);
}

TEST_CASE(
    "A search rejects places off its grid, missing functions and costs below one",
    "[navigation][search][validation]")
{
    const ConnectionFunction leadsOut =
        connectionsFrom({{{0, 0}, {connectionTo(floorOf(8, 0), Traversal::Fly, 1)}}});
    REQUIRE_THROWS_AS(
        findLowestCostRoute(floorOf(0, 0), {1, 0}, TestGrid, leadsOut, zeroHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        findLowestCostRoute(floorOf(-1, 0), {1, 0}, TestGrid, noConnections, zeroHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        findLowestCostRoute(floorOf(0, 0), {1, 0}, {0, 8}, noConnections, zeroHeuristic),
        std::invalid_argument);

    const ConnectionFunction missingConnections;
    const HeuristicFunction missingHeuristic;
    const HeuristicFunction negativeHeuristic = [](Cell, Cell) { return -1; };
    REQUIRE_THROWS_AS(
        findLowestCostRoute(floorOf(0, 0), {1, 0}, TestGrid, missingConnections, zeroHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        findLowestCostRoute(floorOf(0, 0), {1, 0}, TestGrid, noConnections, missingHeuristic),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        findLowestCostRoute(floorOf(0, 0), {1, 0}, TestGrid, noConnections, negativeHeuristic),
        std::invalid_argument);

    const ConnectionFunction costsNothing =
        connectionsFrom({{{0, 0}, {connectionTo(floorOf(1, 0), Traversal::Walk, 0)}}});
    REQUIRE_THROWS_AS(
        findLowestCostRoute(floorOf(0, 0), {1, 0}, TestGrid, costsNothing, zeroHeuristic),
        std::invalid_argument);
    const CostFunction chargesNothing = [](const RouteConnection&) { return 0; };
    REQUIRE_THROWS_AS(
        findLowestCostRoute(
            floorOf(0, 0), {1, 0}, TestGrid, lineUpTo(2), zeroHeuristic, chargesNothing),
        std::invalid_argument);
}

TEST_CASE(
    "A search pauses at a location whose connections are not ready yet",
    "[navigation][search]")
{
    const ConnectionFunction line = lineUpTo(2);
    int connectionQueries = 0;
    const ConnectionFunction notReadyAtOne =
        [&](RouteLocation location) -> std::optional<std::span<const RouteConnection>>
    {
        ++connectionQueries;
        if (location.cell == Cell{1, 0})
        {
            return std::nullopt;
        }
        return line(location);
    };

    const RouteSearchResult paused =
        findLowestCostRoute(floorOf(0, 0), {2, 0}, TestGrid, notReadyAtOne, gridSteps);
    REQUIRE_FALSE(paused.route.has_value());
    REQUIRE(paused.unexpandedLocation == floorOf(1, 0));
    REQUIRE(connectionQueries == 2);
}

TEST_CASE("A search stops at the cheapest location in the goal cell", "[navigation][search]")
{
    const RouteLocation goalWall{{1, 0}, ClimbSurface::LeftWall};
    const ConnectionFunction connections = connectionsFrom(
        {{{0, 0},
          {connectionTo(floorOf(1, 0), Traversal::Walk, 5),
           connectionTo(goalWall, Traversal::Climb, 2)}}});

    const RouteSearchResult result =
        findLowestCostRoute(floorOf(0, 0), {1, 0}, TestGrid, connections, zeroHeuristic);
    REQUIRE(result.route.has_value());
    REQUIRE(endOf(routeOf(result)) == goalWall);
}

TEST_CASE(
    "A goal off the grid is never reached, so the route gets as close as it can",
    "[navigation][search]")
{
    const RouteSearchResult offGrid =
        findLowestCostRoute(floorOf(0, 0), {20, 0}, TestGrid, lineUpTo(7), gridSteps);
    REQUIRE(offGrid.route.has_value());
    REQUIRE(endOf(routeOf(offGrid)) == floorOf(7, 0));
}
