#pragma once

#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/navigation/navigation_fill.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "support/fixed_step.hpp"

namespace tests
{
    // Queues and caches every cell for each platformer NPC profile, as the game does
    // over the first steps of a level.
    inline void prepareNavigationCache(
        const advanced_platformer::TileMap& map,
        advanced_platformer::World& world)
    {
        advanced_platformer::queueNavigationFill(map, world, FixedStepSeconds);
        advanced_platformer::PlatformerConnectionCache& cache = world.platformerConnections();
        while (advanced_platformer::advanceNavigationFill(
                   map, cache, advanced_platformer::NavigationFillTicksPerStep) > 0)
        {
        }
    }
}
