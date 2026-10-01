#pragma once

#include "advanced_platformer/physics/collision.hpp"

namespace advanced_platformer
{
    class TileMap;
    struct Body;
    struct InputIntentions;
    enum class Facing;

    struct FlyingMovement
    {
        float speed = 60.0F;
    };

    CollisionContacts updateFlyingMovement(
        const TileMap& map,
        Body& body,
        const FlyingMovement& movement,
        const InputIntentions& intentions,
        float deltaTime);
}
