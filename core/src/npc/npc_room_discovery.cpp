#include "advanced_platformer/npc/npc_room_discovery.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/navigation/platformer_cells.hpp"
#include "advanced_platformer/world/level_room.hpp"
#include "advanced_platformer/world/tile_map.hpp"

namespace advanced_platformer
{
    namespace
    {
        bool inside(const LevelRoom& room, Cell cell)
        {
            return contains(room.size, {cell.x - room.origin.x, cell.y - room.origin.y});
        }

        std::optional<Cell> restingCellNearest(
            const TileMap& map,
            const LevelRoom& room,
            Cell cell,
            glm::vec2 bodySize)
        {
            std::optional<Cell> nearest;
            int nearestDistance = 0;
            for (int y = room.origin.y; y < room.origin.y + room.size.height; ++y)
            {
                for (int x = room.origin.x; x < room.origin.x + room.size.width; ++x)
                {
                    const Cell candidate{x, y};
                    if (!canStandAt(map, candidate, bodySize))
                    {
                        continue;
                    }
                    const int dx = x - cell.x;
                    const int dy = y - cell.y;
                    const int distance = (dx * dx) + (dy * dy);
                    if (!nearest.has_value() || distance < nearestDistance)
                    {
                        nearest = candidate;
                        nearestDistance = distance;
                    }
                }
            }
            return nearest;
        }

        std::vector<Cell> openingsBetween(
            const TileMap& map,
            const LevelRoom& from,
            const LevelRoom& to)
        {
            const Cell first{
                std::max(from.origin.x, to.origin.x), std::max(from.origin.y, to.origin.y)};
            const Cell last{
                std::min(from.origin.x + from.size.width, to.origin.x + to.size.width) - 1,
                std::min(from.origin.y + from.size.height, to.origin.y + to.size.height) - 1};
            const bool column = first.x == last.x && last.y - first.y >= 2;
            const bool row = first.y == last.y && last.x - first.x >= 2;
            std::vector<Cell> middles;
            if (!column && !row)
            {
                return middles;
            }
            const Cell step = column ? Cell{0, 1} : Cell{1, 0};
            std::vector<Cell> opening;
            const auto closeOpening = [&]
            {
                if (!opening.empty())
                {
                    middles.push_back(opening[opening.size() / 2]);
                    opening.clear();
                }
            };
            for (Cell cell{first.x + step.x, first.y + step.y}; cell != last;
                 cell = {cell.x + step.x, cell.y + step.y})
            {
                if (map.contains(cell) && !map.blocksMovement(cell))
                {
                    opening.push_back(cell);
                }
                else
                {
                    closeOpening();
                }
            }
            closeOpening();
            return middles;
        }

        void findDoors(
            RoomDiscovery& discovery,
            const std::vector<LevelRoom>& rooms,
            const TileMap& map,
            glm::vec2 bodySize)
        {
            discovery.unexploredDoors.clear();
            discovery.unexploredDoorRooms.clear();
            discovery.enteredLinks.assign(rooms.size(), {});
            for (std::size_t from = 0; from < rooms.size(); ++from)
            {
                if (!discovery.entered[from])
                {
                    continue;
                }
                for (std::size_t to = 0; to < rooms.size(); ++to)
                {
                    if (to == from)
                    {
                        continue;
                    }
                    const std::vector<Cell> openings = openingsBetween(map, rooms[from], rooms[to]);
                    if (openings.empty())
                    {
                        continue;
                    }
                    if (discovery.entered[to])
                    {
                        discovery.enteredLinks[from].push_back(to);
                        continue;
                    }
                    for (const Cell middle : openings)
                    {
                        if (const std::optional<Cell> resting =
                                restingCellNearest(map, rooms[to], middle, bodySize);
                            resting.has_value())
                        {
                            discovery.unexploredDoors.push_back(
                                feetInCell(map.tileSize(), *resting));
                            discovery.unexploredDoorRooms.push_back(from);
                        }
                    }
                }
            }
        }
    }

    void discoverRooms(
        RoomDiscovery& discovery,
        const std::vector<LevelRoom>& rooms,
        const TileMap& map,
        const Aabb& body)
    {
        bool changed = discovery.breaksWhenFound != map.brokenCells().size();
        if (discovery.entered.size() != rooms.size())
        {
            discovery.entered.assign(rooms.size(), false);
            changed = true;
        }
        const Cell feet = cellAtFeet(map.tileSize(), feetOf(body));
        for (std::size_t room = 0; room < rooms.size(); ++room)
        {
            if (!discovery.entered[room] && inside(rooms[room], feet))
            {
                discovery.entered[room] = true;
                changed = true;
            }
        }
        if (changed)
        {
            discovery.breaksWhenFound = map.brokenCells().size();
            findDoors(discovery, rooms, map, body.size);
        }
    }

    bool discovered(const RoomDiscovery& discovery, const std::vector<LevelRoom>& rooms, Cell cell)
    {
        if (rooms.empty())
        {
            return true;
        }
        for (std::size_t room = 0; room < rooms.size() && room < discovery.entered.size(); ++room)
        {
            if (discovery.entered[room] && inside(rooms[room], cell))
            {
                return true;
            }
        }
        return false;
    }

    std::vector<int> roomsToUnexploredDoors(
        const RoomDiscovery& discovery,
        const std::vector<LevelRoom>& rooms,
        Cell feet)
    {
        constexpr int Unreached = -1;
        std::vector<int> away(rooms.size(), Unreached);
        std::vector<std::size_t> queue;
        for (std::size_t room = 0; room < rooms.size() && room < discovery.entered.size(); ++room)
        {
            if (discovery.entered[room] && inside(rooms[room], feet))
            {
                away[room] = 0;
                queue.push_back(room);
            }
        }
        for (std::size_t next = 0; next < queue.size(); ++next)
        {
            const std::size_t room = queue[next];
            for (const std::size_t linked : discovery.enteredLinks[room])
            {
                if (away[linked] == Unreached)
                {
                    away[linked] = away[room] + 1;
                    queue.push_back(linked);
                }
            }
        }
        std::vector<int> result;
        result.reserve(discovery.unexploredDoorRooms.size());
        for (const std::size_t room : discovery.unexploredDoorRooms)
        {
            result.push_back(away[room] == Unreached ? static_cast<int>(rooms.size()) : away[room]);
        }
        return result;
    }
}
