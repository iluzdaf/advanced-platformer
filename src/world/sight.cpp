#include "advanced_platformer/world/sight.hpp"

#include <glm/vec2.hpp>

#include "advanced_platformer/physics/segment_cast.hpp"

namespace advanced_platformer
{
    bool lineOfSight(const TileMap& map, glm::vec2 from, glm::vec2 to)
    {
        return !segmentCastSightBlockingTiles(map, from, to).has_value();
    }
}
