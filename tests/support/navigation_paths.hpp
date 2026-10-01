#pragma once

#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/navigation/navigation_fill.hpp"
#include "advanced_platformer/navigation/route.hpp"
#include "advanced_platformer/navigation/navigation_path.hpp"
#include "advanced_platformer/navigation/platformer_traversal_profile.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "support/tile_size.hpp"

namespace tests
{
    // The waypoints a route of floor steps from the start cell gives, for tests that
    // assemble a path from simulated connections.
    inline advanced_platformer::NavigationPath floorPath(
        advanced_platformer::Cell start,
        std::vector<advanced_platformer::RouteStep> steps)
    {
        using advanced_platformer::feetInCell;

        advanced_platformer::NavigationPath path{feetInCell(TileSize, start), {}};
        for (advanced_platformer::RouteStep& step : steps)
        {
            path.waypoints.push_back(
                {feetInCell(TileSize, step.destination.cell),
                 step.traversal,
                 std::move(step.inputs)});
        }
        return path;
    }

    // Queues every cell of the map for the profile and fills them all, as the game
    // does for each NPC profile over the first steps of a level.
    inline void fillConnections(
        const advanced_platformer::TileMap& map,
        advanced_platformer::PlatformerConnectionCache& cache,
        const advanced_platformer::PlatformerTraversalProfile& profile)
    {
        for (int row = 0; row < map.height(); ++row)
        {
            for (int column = 0; column < map.width(); ++column)
            {
                cache.queue({column, row}, profile);
            }
        }
        while (advanced_platformer::advanceNavigationFill(
                   map, cache, advanced_platformer::NavigationFillTicksPerStep) > 0)
        {
        }
    }
}
