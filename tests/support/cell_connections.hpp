#pragma once

#include <optional>
#include <stdexcept>
#include <vector>

#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/navigation/platformer_connections.hpp"
#include "advanced_platformer/navigation/platformer_traversal_profile.hpp"
#include "advanced_platformer/navigation/route.hpp"
#include "advanced_platformer/world/tile_map.hpp"

namespace tests
{
    // What caching one cell into an empty cache stored, and the ticks it simulated.
    struct CellConnections
    {
        std::vector<advanced_platformer::RouteConnection> connections;
        advanced_platformer::CellRange footprint;
        int simulatedTicks = 0;
    };

    // The connections leaving a cell, simulated with no walks to reuse.
    inline CellConnections connectionsFrom(
        const advanced_platformer::TileMap& map,
        advanced_platformer::Cell cell,
        const advanced_platformer::PlatformerTraversalProfile& profile)
    {
        advanced_platformer::PlatformerConnectionCache cache;
        const int simulatedTicks =
            advanced_platformer::cachePlatformerConnections(map, cache, cell, profile);
        const std::vector<advanced_platformer::RouteConnection>* connections =
            cache.cachedConnections(cell, profile);
        const std::optional<advanced_platformer::CellRange> footprint =
            cache.cachedFootprint(cell, profile);
        if (connections == nullptr || !footprint.has_value())
        {
            throw std::logic_error("Caching a cell stored nothing for it");
        }
        return {*connections, *footprint, simulatedTicks};
    }
}
