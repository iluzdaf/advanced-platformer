#include "advanced_platformer/navigation/platformer_cells.hpp"

#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/route.hpp"
#include "advanced_platformer/physics/collision.hpp"
#include "advanced_platformer/world/tile_map.hpp"

namespace advanced_platformer
{
    namespace
    {
        bool bodyFits(const TileMap& map, const Aabb& bounds)
        {
            if (bounds.topLeft.x < 0.0F || rightOf(bounds) > map.pixelWidth() ||
                bottomOf(bounds) > map.pixelHeight())
            {
                return false;
            }
            const CellRange cells = cellsCovered(map.tileSize(), bounds);
            for (int row = cells.first.y; row <= cells.last.y; ++row)
            {
                for (int column = cells.first.x; column <= cells.last.x; ++column)
                {
                    if (map.blocksMovement({column, row}))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        void requireBodySize(glm::vec2 bodySize)
        {
            if (!isFinitePositive(bodySize))
            {
                throw std::invalid_argument("Navigation body size must be finite and positive");
            }
        }
    }

    bool canStandAt(const TileMap& map, Cell cell, glm::vec2 bodySize)
    {
        return canOccupy(map, {cell, ClimbSurface::None}, bodySize);
    }

    bool canClimbAt(const TileMap& map, Cell cell, glm::vec2 bodySize)
    {
        for (const ClimbSurface surface :
             {ClimbSurface::LeftWall, ClimbSurface::RightWall, ClimbSurface::Ceiling})
        {
            if (canOccupy(map, {cell, surface}, bodySize))
            {
                return true;
            }
        }
        return false;
    }

    Aabb boundsAtSurface(int tileSize, RouteLocation location, glm::vec2 bodySize)
    {
        Aabb bounds = boxInCell(tileSize, location.cell, bodySize);
        const float left = static_cast<float>(location.cell.x * tileSize);
        const float top = static_cast<float>(location.cell.y * tileSize);
        switch (location.surface)
        {
        case ClimbSurface::None:
            break;
        case ClimbSurface::LeftWall:
            bounds.topLeft.x = left;
            break;
        case ClimbSurface::RightWall:
            bounds.topLeft.x = left + static_cast<float>(tileSize) - bodySize.x;
            break;
        case ClimbSurface::Ceiling:
            bounds.topLeft.y = top;
            break;
        }
        return bounds;
    }

    bool canOccupy(const TileMap& map, RouteLocation location, glm::vec2 bodySize)
    {
        requireBodySize(bodySize);
        if (!map.contains(location.cell))
        {
            return false;
        }
        const Aabb bounds = boundsAtSurface(map.tileSize(), location, bodySize);
        if (!bodyFits(map, bounds))
        {
            return false;
        }
        if (location.surface == ClimbSurface::None)
        {
            return map.blocksMovement({location.cell.x, location.cell.y + 1});
        }
        return touchesClimbable(map, bounds, location.surface);
    }

    std::vector<RouteLocation> climbDestinationsFrom(RouteLocation from)
    {
        const Cell cell = from.cell;
        switch (from.surface)
        {
        case ClimbSurface::None:
            return {{cell, ClimbSurface::LeftWall}, {cell, ClimbSurface::RightWall}};
        case ClimbSurface::LeftWall:
        case ClimbSurface::RightWall:
            return {
                {cell, ClimbSurface::None},
                {{cell.x, cell.y - 1}, from.surface},
                {{cell.x, cell.y + 1}, from.surface},
                {cell, ClimbSurface::Ceiling}};
        case ClimbSurface::Ceiling:
            return {
                {{cell.x - 1, cell.y}, ClimbSurface::Ceiling},
                {{cell.x + 1, cell.y}, ClimbSurface::Ceiling},
                {cell, ClimbSurface::LeftWall},
                {cell, ClimbSurface::RightWall}};
        }
        return {};
    }
}
