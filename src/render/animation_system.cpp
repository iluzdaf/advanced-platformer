#include "advanced_platformer/render/animation_system.hpp"

#include <optional>
#include <stdexcept>
#include <variant>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_attacks.hpp"
#include "advanced_platformer/combat/attack.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/movement/pounce.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/render/animation.hpp"
#include "advanced_platformer/world/world.hpp"

namespace advanced_platformer
{
    namespace
    {
        void noteAttack(const Attack& attack, AnimationState& state)
        {
            if (const auto* bite = std::get_if<BiteAttack>(&attack))
            {
                state.biting = state.biting || bite->phase != BitePhase::Ready;
            }
            else if (const auto* weapon = std::get_if<RangedWeapon>(&attack))
            {
                state.shooting = state.shooting || weapon->phase == RangedPhase::Shoot;
            }
            else if (const auto* pounce = std::get_if<Pounce>(&attack))
            {
                state.pouncing = state.pouncing || pounce->phase == PouncePhase::Airborne;
            }
        }

        AnimationState animationStateOf(const Actor& actor)
        {
            AnimationState state;
            state.dying = actor.life == LifeState::Dying;
            for (const AttackSlot slot : AttackSlots)
            {
                const std::optional<Attack>& attack = attackIn(actor, slot);
                if (attack.has_value())
                {
                    noteAttack(*attack, state);
                }
            }
            const ClimbSurface surface =
                actor.surfaceClimb.has_value() ? actor.surfaceClimb->surface : ClimbSurface::None;
            state.grounded = surface != ClimbSurface::None ||
                             !actor.platformerMovement.has_value() ||
                             actor.platformerMovement->grounded;
            state.velocity = actor.body.velocity;
            return state;
        }

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
                const AnimationName selected =
                    selectAnimation(actor.animator->animationSet, animationStateOf(actor));
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
