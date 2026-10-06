#pragma once

namespace advanced_platformer
{
    class TileMap;
    class World;

    // Checks actor spawns, the player respawn, and patrol endpoints against the tile map.
    // Each needs clearance. A platformer's spawn and respawn also need ground support,
    // and so do its patrol endpoints unless it can climb.
    void validateLevelActors(const TileMap& map, const World& world, int level);

    // Whether the player, from its respawn, can reach the exit with the moves it has,
    // searched over the same connections navigation uses at stepSeconds. Only the cells
    // the search visits are simulated, into a cache of its own, so the world is left as
    // it was. The exit's requirement is ignored, and so is anything the player could
    // break on the way, which navigation does not model.
    bool playerCanReachExit(const TileMap& map, const World& world, float stepSeconds);
}
