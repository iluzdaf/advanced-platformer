#pragma once

#include "advanced_platformer/movement/surface_climb.hpp"

namespace advanced_platformer
{
    class TileMap;
    class World;
    struct Aabb;
    struct Actor;
    struct NpcBrain;

    const Actor* livingTarget(const World& world, const NpcBrain& brain);

    bool onSameGroundRun(const TileMap& map, const Aabb& observer, const Aabb& target);
    bool onSameClimbSurface(
        const TileMap& map,
        const Aabb& climber,
        ClimbSurface surface,
        const Aabb& target);
    void updateNpcSenses(const TileMap& map, World& world, float deltaTime);
}
