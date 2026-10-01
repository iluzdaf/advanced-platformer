#pragma once

#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"

namespace advanced_platformer
{
    class World;
    class WorldRequests;

    Aabb biteHitbox(const Aabb& actorBounds, const BiteAttack& bite, Facing facing);
    void updateAttacks(World& world, WorldRequests& requests, float deltaTime);
}
