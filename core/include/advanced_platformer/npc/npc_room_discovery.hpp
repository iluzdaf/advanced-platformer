#pragma once

#include <cstddef>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/world/level_room.hpp"

namespace advanced_platformer
{
    class TileMap;

    struct RoomDiscovery
    {
        std::vector<bool> entered;
        std::vector<glm::vec2> unexploredDoors;
        std::size_t breaksWhenFound = 0;
    };

    void discoverRooms(
        RoomDiscovery& discovery,
        const std::vector<LevelRoom>& rooms,
        const TileMap& map,
        const Aabb& body);

    bool discovered(const RoomDiscovery& discovery, const std::vector<LevelRoom>& rooms, Cell cell);
}
