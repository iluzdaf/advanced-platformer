#pragma once

namespace advanced_platformer
{
    class TileMap;
    class World;
    class WorldRequests;

    void updateTouchDamage(const TileMap& map, const World& world, WorldRequests& requests);
}
