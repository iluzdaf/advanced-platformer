#include "advanced_platformer/combat/touch_damage.hpp"

#include <optional>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/physics/collision.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "advanced_platformer/world/world_requests.hpp"

namespace advanced_platformer
{
    namespace
    {
        bool hurtRecently(const Actor& actor, const TouchDamage& touch, double now)
        {
            return actor.lastDamageTimeSeconds.has_value() &&
                   now - *actor.lastDamageTimeSeconds < touch.repeatSeconds;
        }
    }

    void updateTouchDamage(const TileMap& map, const World& world, WorldRequests& requests)
    {
        const double now = world.simulationTimeSeconds();
        for (const Actor& actor : world.actors())
        {
            if (actor.life != LifeState::Alive || !actor.health.has_value())
            {
                continue;
            }
            const std::optional<TouchDamage> touch = touchDamageOverlapping(map, actor.body.bounds);
            if (!touch.has_value() || hurtRecently(actor, *touch, now))
            {
                continue;
            }
            std::optional<glm::vec2> knockback;
            if (touch->lift > 0.0F)
            {
                knockback = glm::vec2{0.0F, -touch->lift};
            }
            requests.damage(actor.id, touch->damage, knockback);
        }
    }
}
