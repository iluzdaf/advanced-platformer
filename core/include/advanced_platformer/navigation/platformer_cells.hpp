#pragma once

#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/route.hpp"

namespace advanced_platformer
{
    class TileMap;

    bool canStandAt(const TileMap& map, Cell cell, glm::vec2 bodySize);

    bool canClimbAt(const TileMap& map, Cell cell, glm::vec2 bodySize);

    Aabb boundsAtSurface(int tileSize, RouteLocation location, glm::vec2 bodySize);

    bool canOccupy(const TileMap& map, RouteLocation location, glm::vec2 bodySize);

    std::vector<RouteLocation> climbDestinationsFrom(RouteLocation from);
}
