#pragma once

#include <cstddef>

#include "advanced_platformer/actor/actor_id.hpp"

namespace advanced_platformer
{
    struct Actor;
    struct ItemDefinition;
    class World;

    bool useItem(World& world, ActorId actor, std::size_t slot);
    bool hasAmmo(const World& world, const Actor& actor);
    void equipWeapon(Actor& actor, const ItemDefinition& weapon);
    void holsterWeapon(Actor& actor);
}
