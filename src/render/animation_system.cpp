#include "advanced_platformer/render/animation_system.hpp"

#include <algorithm>
#include <optional>
#include <variant>
#include <stdexcept>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_attacks.hpp"
#include "advanced_platformer/combat/attack.hpp"
#include "advanced_platformer/movement/pounce.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/render/animation.hpp"
#include "advanced_platformer/world/world.hpp"

namespace advanced_platformer
{
    namespace
    {
        bool attackAnimates(const Attack& attack)
        {
            if (const auto* bite = std::get_if<BiteAttack>(&attack))
            {
                return bite->phase != BitePhase::Ready;
            }
            if (const auto* weapon = std::get_if<RangedWeapon>(&attack))
            {
                return weapon->phase == RangedPhase::Shoot;
            }
            if (const auto* pounce = std::get_if<Pounce>(&attack))
            {
                return pounce->phase == PouncePhase::Airborne;
            }
            return false;
        }
    }

    namespace
    {
        void updateActorAnimations(World& world, float deltaTime)
        {
            for (Actor& actor : world.actors())
            {
                if (!actor.animator.has_value())
                {
                    continue;
                }
                if ((!actor.platformerMovement.has_value() && !actor.flyingMovement.has_value()) ||
                    !actor.sprite.has_value())
                {
                    throw std::logic_error("An animated actor is missing a required component");
                }

                const bool attacking = std::ranges::any_of(
                    AttackSlots,
                    [&actor](AttackSlot slot)
                    {
                        const std::optional<Attack>& attack = attackIn(actor, slot);
                        return attack.has_value() && attackAnimates(*attack);
                    });
                const ClimbSurface surface = actor.surfaceClimb.has_value()
                                                 ? actor.surfaceClimb->surface
                                                 : ClimbSurface::None;
                const bool grounded = surface != ClimbSurface::None ||
                                      !actor.platformerMovement.has_value() ||
                                      actor.platformerMovement->grounded;
                const AnimationName selected = selectActorAnimation(
                    actor.life == LifeState::Dying, attacking, grounded, actor.body.velocity);
                updateAnimation(*actor.animator, *actor.sprite, selected, deltaTime);
            }
        }
    }

    void updateWorldAnimations(World& world, float deltaTime)
    {
        requireSeconds(deltaTime, "Animations time step");
        updateActorAnimations(world, deltaTime);
    }
}
