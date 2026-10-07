#pragma once

#include <optional>

#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/navigation/platformer_traversal_profile.hpp"

namespace advanced_platformer
{
    class PlatformerConnectionCache;
    class TileMap;

    struct WalkSimulationResult
    {
        int columns = 0;
        std::optional<int> cost;
        CellRange sweep;
        int simulatedTicks = 0;
    };

    int cachePlatformerConnections(
        const TileMap& map,
        PlatformerConnectionCache& cache,
        Cell cell,
        const PlatformerTraversalProfile& profile);
}
