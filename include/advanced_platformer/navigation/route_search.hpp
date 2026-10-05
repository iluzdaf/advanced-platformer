#pragma once

#include <functional>
#include <optional>
#include <span>

#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/navigation/route.hpp"

namespace advanced_platformer
{
    using ConnectionFunction =
        std::function<std::optional<std::span<const RouteConnection>>(RouteLocation location)>;
    using HeuristicFunction = std::function<int(Cell cell, Cell goal)>;
    using CostFunction = std::function<int(const RouteConnection& connection)>;

    struct RouteSearchResult
    {
        std::optional<Route> route;
        std::optional<RouteLocation> unexpandedLocation;
    };

    RouteSearchResult findLowestCostRoute(
        RouteLocation start,
        Cell goal,
        GridSize grid,
        const ConnectionFunction& connections,
        const HeuristicFunction& heuristic,
        const CostFunction& costOf = {});
}
