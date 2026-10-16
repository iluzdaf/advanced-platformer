#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "advanced_platformer/math/aabb.hpp"

namespace advanced_platformer
{
    class TileMap;
    struct TouchDamage;

    struct CollisionContacts
    {
        bool left = false;
        bool right = false;
        bool ground = false;
        bool ceiling = false;
    };

    CollisionContacts moveAndCollide(const TileMap& map, Aabb& bounds, glm::vec2 displacement);

    CollisionContacts touchingSurfaces(const TileMap& map, const Aabb& bounds);

    CollisionContacts touchingClimbableSurfaces(const TileMap& map, const Aabb& bounds);

    std::optional<TouchDamage> touchDamageOverlapping(const TileMap& map, const Aabb& bounds);
}
