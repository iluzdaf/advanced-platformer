#include "advanced_platformer/actor/actor_validation.hpp"

#include "advanced_platformer/npc/npc_state_machine.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_attacks.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/combat/attack.hpp"
#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/movement/pounce.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"

namespace advanced_platformer
{
    namespace
    {
        void validateIdentity(const Actor& actor)
        {
            if (isValid(actor.id))
            {
                throw std::invalid_argument("World assigns actor IDs");
            }
        }

        void validateBody(const Actor& actor)
        {
            if (!isFinite(actor.body.bounds.topLeft) || !isFinite(actor.body.bounds.size) ||
                !isFinite(actor.body.velocity) || actor.body.bounds.size.x <= 0.0F ||
                actor.body.bounds.size.y <= 0.0F)
            {
                throw std::invalid_argument("Actors require finite positive-sized bodies");
            }
        }

        void validateMovement(const Actor& actor)
        {
            if (actor.platformerMovement.has_value() == actor.flyingMovement.has_value())
            {
                throw std::invalid_argument("Actors require exactly one movement component");
            }
            if (actor.flyingMovement.has_value() &&
                (!isFiniteNonNegative(actor.flyingMovement->config.speed)))
            {
                throw std::invalid_argument(
                    "Flying movement speed must be finite and non-negative");
            }
            if (actor.surfaceClimb.has_value())
            {
                if (!actor.platformerMovement.has_value())
                {
                    throw std::invalid_argument("Surface climbing requires platformer movement");
                }
                validateSurfaceClimbConfig(actor.surfaceClimb->config);
            }
        }

        void validatePresentation(const Actor& actor)
        {
            if (actor.animator.has_value() && !actor.sprite.has_value())
            {
                throw std::invalid_argument("Animated actors require a sprite");
            }
            if (actor.animator.has_value())
            {
                requireSeconds(actor.animator->elapsed, "Actor animation elapsed");
                if (actor.animator->animationSet.clips.empty())
                {
                    throw std::invalid_argument("Actor animation data is invalid");
                }
            }
        }

        void validateKnockback(const std::optional<Knockback>& knockback, const char* what)
        {
            if (knockback.has_value() &&
                (!isFiniteNonNegative(knockback->speed) || !isFiniteNonNegative(knockback->lift)))
            {
                throw std::invalid_argument(
                    std::string(what) + " knockback needs finite, non-negative speed and lift");
            }
        }

        void validateAttack(const Actor& actor, const Attack& attack)
        {
            if (const auto* weapon = std::get_if<RangedWeapon>(&attack))
            {
                requireSeconds(weapon->phaseTimeRemaining, "Ranged weapon phase time remaining");
                if (weapon->damage <= 0 || !isFinitePositive(weapon->projectileSize) ||
                    !isFinitePositive(weapon->projectileSpeed) ||
                    !isFinitePositive(weapon->projectileLifetime) ||
                    !isFinitePositive(weapon->shootDuration) ||
                    !isFinitePositive(weapon->recoveryDuration) ||
                    !isFinitePositive(weapon->projectileSprite.region.size))
                {
                    throw std::invalid_argument("Actor ranged weapon data is invalid");
                }
            }
            else if (const auto* bite = std::get_if<BiteAttack>(&attack))
            {
                requireSeconds(bite->phaseTimeRemaining, "Bite phase time remaining");
                if (bite->damage <= 0 || !isFinitePositive(bite->hitboxSize) ||
                    !isFiniteNonNegative(bite->reach) || !isFinitePositive(bite->windupDuration) ||
                    !isFinitePositive(bite->activeDuration) ||
                    !isFinitePositive(bite->recoveryDuration))
                {
                    throw std::invalid_argument("Actor bite data is invalid");
                }
            }
            else if (const auto* contact = std::get_if<ContactDamage>(&attack))
            {
                if (contact->damage <= 0)
                {
                    throw std::invalid_argument("Actor contact damage must be positive");
                }
                validateKnockback(contact->knockback, "Actor contact");
            }
            else if (const auto* pounce = std::get_if<Pounce>(&attack))
            {
                if (!actor.platformerMovement.has_value())
                {
                    throw std::invalid_argument("Pouncing requires platformer movement");
                }
                validatePounceConfig(pounce->config);
                requireSeconds(pounce->phaseTimeRemaining, "Pounce phase time remaining");
                if (pounce->config.damage <= 0)
                {
                    throw std::invalid_argument("Actor pounce damage must be positive");
                }
                validateKnockback(pounce->config.knockback, "Actor pounce");
            }
        }

        void validateCombat(const Actor& actor)
        {
            int attacks = 0;
            int pounces = 0;
            for (const AttackSlot slot : AttackSlots)
            {
                const std::optional<Attack>& attack = attackIn(actor, slot);
                if (!attack.has_value())
                {
                    continue;
                }
                ++attacks;
                pounces += static_cast<int>(std::holds_alternative<Pounce>(*attack));
                validateAttack(actor, *attack);
            }
            if (pounces > 1)
            {
                throw std::invalid_argument("An actor can pounce from only one attack slot");
            }
            if (attacks > 0 && actor.team == Team::Neutral)
            {
                throw std::invalid_argument("Actors with attacks require a non-neutral team");
            }
            if (actor.health.has_value() &&
                (actor.health->maximum <= 0 || actor.health->current < 0 ||
                 actor.health->current > actor.health->maximum))
            {
                throw std::invalid_argument("Actor health must be within zero and its maximum");
            }
        }

        void validateNpc(const Actor& actor)
        {
            const bool hasAnyNpcComponent =
                actor.brain.has_value() || actor.senses.has_value() ||
                actor.perception.has_value() || actor.patrol.has_value() ||
                actor.pathFollower.has_value() || actor.machine.has_value();
            const bool hasRequiredNpcComponents =
                actor.brain.has_value() && actor.perception.has_value() &&
                actor.senses.has_value() && actor.pathFollower.has_value() &&
                actor.machine.has_value();
            if (hasAnyNpcComponent && !hasRequiredNpcComponents)
            {
                throw std::invalid_argument(
                    "NPC actors require a brain, perception, senses, a path follower, and a state "
                    "machine");
            }
            if (actor.machine.has_value())
            {
                validateNpcStateMachine(actor.machine->definition);
                if (actor.machine->active >= actor.machine->definition.states.size() ||
                    actor.machine->heldFor.size() != actor.machine->definition.transitions.size())
                {
                    throw std::invalid_argument("An NPC state machine must be started");
                }
                requireSeconds(actor.machine->stateElapsed, "NPC machine state elapsed");
            }
            if (actor.brain.has_value())
            {
                requireSeconds(actor.brain->targetMemoryRemaining, "NPC target memory remaining");
                if (!isFinite(actor.brain->lastKnownTargetFeet))
                {
                    throw std::invalid_argument("NPC brain runtime data is invalid");
                }
            }
            if (actor.senses.has_value())
            {
                requireSeconds(actor.senses->targetMemoryDuration, "NPC target memory duration");
                requireSeconds(actor.senses->searchDuration, "NPC search duration");
                if (!isFiniteNonNegative(actor.senses->noticeDistance) ||
                    !isFiniteNonNegative(actor.senses->standoffDistance))
                {
                    throw std::invalid_argument("NPC senses data is invalid");
                }
            }
            if (actor.patrol.has_value() &&
                (!isFinite(actor.patrol->firstFeet) || !isFinite(actor.patrol->secondFeet)))
            {
                throw std::invalid_argument("NPC patrol endpoints must be finite");
            }
        }

        void validatePathFollower(const Actor& actor)
        {
            if (actor.pathFollower.has_value())
            {
                requireSeconds(actor.pathFollower->programElapsed, "NPC path program elapsed");
            }
        }
    }

    void validateActor(const Actor& actor)
    {
        validateIdentity(actor);
        validateBody(actor);
        validateMovement(actor);
        validatePresentation(actor);
        validateCombat(actor);
        validateNpc(actor);
        validatePathFollower(actor);
    }
}
