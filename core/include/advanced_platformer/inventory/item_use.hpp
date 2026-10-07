#pragma once

#include <cstddef>

#include "advanced_platformer/actor/actor_id.hpp"

namespace advanced_platformer
{
    class World;

    bool useItem(World& world, ActorId actor, std::size_t slot);
}
