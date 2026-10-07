#pragma once

namespace advanced_platformer
{
    class TileMap;
    class World;

    void validateLevelActors(const TileMap& map, const World& world, int level);

    bool playerCanReachExit(const TileMap& map, const World& world, float stepSeconds);
}
