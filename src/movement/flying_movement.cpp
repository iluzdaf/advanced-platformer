#include "advanced_platformer/movement/flying_movement.hpp"

#include <stdexcept>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/physics/body.hpp"
#include "advanced_platformer/physics/collision.hpp"
#include "advanced_platformer/world/tile_map.hpp"

namespace advanced_platformer
{
    CollisionContacts updateFlyingMovement(
        const TileMap& map,
        Body& body,
        const FlyingMovement& movement,
        const InputIntentions& intentions,
        float deltaTime)
    {
        requireSeconds(deltaTime, "Flying movement time step");
        if (!isFiniteNonNegative(movement.speed) || !isFinite(intentions.direction))
        {
            throw std::invalid_argument(
                "Flying movement requires a finite, non-negative speed and finite intentions");
        }

        glm::vec2 direction = intentions.direction;
        const float length = glm::length(direction);
        if (length > 1.0F)
        {
            direction /= length;
        }

        body.velocity = direction * movement.speed;
        return moveBody(map, body, deltaTime);
    }
}
