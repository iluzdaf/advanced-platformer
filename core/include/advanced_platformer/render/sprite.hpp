#pragma once

#include <glm/vec2.hpp>

#include "advanced_platformer/math/aabb.hpp"

namespace advanced_platformer
{
    struct SpriteRegion
    {
        glm::vec2 position = {0.0F, 0.0F};
        glm::vec2 size = {0.0F, 0.0F};
    };

    enum class SpriteAnchor
    {
        BodyFeet,
        BodyCenter
    };

    struct Sprite
    {
        int textureId = 0;
        SpriteRegion region;
        SpriteAnchor anchor = SpriteAnchor::BodyFeet;
    };

    Aabb spriteBounds(const Aabb& bodyBounds, const Sprite& sprite);
}
