#pragma once

namespace advanced_platformer
{
    class TileMap;
    class World;

    // Presents the simulated world without changing gameplay. Runs after the simulation.
    void updateWorldPresentation(const TileMap& map, World& world, float deltaTime);
}
