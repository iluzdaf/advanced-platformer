#include "advanced_platformer/render/sprite.hpp"

#include <stdexcept>

#include "advanced_platformer/math/aabb.hpp"

namespace advanced_platformer
{
    Aabb spriteBounds(const Aabb& bodyBounds, const Sprite& sprite)
    {
        switch (sprite.anchor)
        {
        case SpriteAnchor::BodyFeet: {
            const glm::vec2 bodyFeet = feetOf(bodyBounds);
            return {
                {bodyFeet.x - sprite.region.size.x * 0.5F, bodyFeet.y - sprite.region.size.y},
                sprite.region.size};
        }
        case SpriteAnchor::BodyCenter:
            return boxCenteredOn(centerOf(bodyBounds), sprite.region.size);
        }

        throw std::invalid_argument("Sprite anchor is invalid");
    }
}
