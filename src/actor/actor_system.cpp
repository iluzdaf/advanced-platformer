#include "advanced_platformer/actor/actor_system.hpp"

#include <stdexcept>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_attacks.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/combat/attack.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/movement/flying_movement.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/pounce.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"

namespace advanced_platformer
{
    void updateActorMovement(const TileMap& map, World& world, float deltaTime)
    {
        for (Actor& actor : world.actors())
        {
            const InputIntentions intentions =
                actor.life == LifeState::Alive ? actor.intentions : InputIntentions{};
            if (actor.platformerMovement.has_value())
            {
                PlatformerMovement& movement = *actor.platformerMovement;
                const bool wasGrounded = movement.grounded;
                if (Pounce* pounce = findAttack<Pounce>(actor))
                {
                    SurfaceClimb* climb =
                        actor.surfaceClimb.has_value() ? &*actor.surfaceClimb : nullptr;
                    const bool pressed = attackPressed(
                        intentions, slotHolding<Pounce>(actor).value_or(AttackSlot::Primary));
                    updatePounceMovement(
                        map, actor.body, movement, climb, *pounce, intentions, pressed, deltaTime);
                }
                else if (actor.surfaceClimb.has_value())
                {
                    updateSurfaceClimbMovement(
                        map, actor.body, movement, *actor.surfaceClimb, intentions, deltaTime);
                }
                else
                {
                    updatePlatformerMovement(map, actor.body, movement, intentions, deltaTime);
                }
                if (!wasGrounded && movement.grounded && actor.life == LifeState::Alive)
                {
                    world.emitNoise({actor.id, feetOf(actor.body.bounds), NoiseKind::Landing});
                }
            }
            else if (actor.flyingMovement.has_value())
            {
                updateFlyingMovement(map, actor.body, *actor.flyingMovement, intentions, deltaTime);
            }
            else
            {
                throw std::logic_error("An actor has no movement component");
            }

            actor.facing = facingFor(intentions, actor.facing);
        }
    }
}
