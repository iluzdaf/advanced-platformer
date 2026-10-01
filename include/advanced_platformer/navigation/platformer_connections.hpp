#pragma once

#include <optional>

#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/navigation/platformer_traversal_profile.hpp"

namespace advanced_platformer
{
    class PlatformerConnectionCache;
    class TileMap;

    // What a walk of some number of cells along a floor cost when it was simulated, and
    // the cells its simulation swept, as offsets from the cell it started in. No cost
    // when the body could not reach the cell and stop within the simulation limit.
    // Simulated ticks describe the original walk, not work done when it is reused.
    struct WalkSimulationResult
    {
        // Signed distance from the start cell; also the walk cache's key.
        int columns = 0;
        std::optional<int> cost;
        CellRange sweep;
        int simulatedTicks = 0;
    };

    // Simulates the connections leaving a cell, reusing the flat walks the cache already
    // holds, and stores them in the cache together with the walks it newly simulated.
    // Returns the movement ticks it simulated; reused walks add none.
    int cachePlatformerConnections(
        const TileMap& map,
        PlatformerConnectionCache& cache,
        Cell cell,
        const PlatformerTraversalProfile& profile);
}
