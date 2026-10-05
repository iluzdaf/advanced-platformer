#include "actor_definition.hpp"

#include "animation_catalog.hpp"
#include "machine_catalog.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <variant>
#include <string>
#include <utility>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_attacks.hpp"
#include "advanced_platformer/actor/actor_validation.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/combat/attack.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/movement/flying_movement.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/pounce.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/render/animation.hpp"

namespace advanced_platformer
{
    namespace
    {
        std::optional<Attack> freshAttack(const std::optional<Attack>& definition)
        {
            if (!definition.has_value())
            {
                return std::nullopt;
            }
            Attack attack = *definition;
            if (auto* bite = std::get_if<BiteAttack>(&attack))
            {
                bite->phase = BitePhase::Ready;
                bite->phaseTimeRemaining = 0.0F;
                bite->actorsHit.clear();
            }
            else if (auto* weapon = std::get_if<RangedWeapon>(&attack))
            {
                weapon->phase = RangedPhase::Ready;
                weapon->phaseTimeRemaining = 0.0F;
                weapon->lastFiredTimeSeconds = std::nullopt;
            }
            else if (auto* contact = std::get_if<ContactDamage>(&attack))
            {
                contact->active = false;
                contact->actorsHit.clear();
            }
            else if (auto* pounce = std::get_if<Pounce>(&attack))
            {
                pounce->phase = PouncePhase::Ready;
                pounce->phaseTimeRemaining = 0.0F;
                pounce->launchedFrom = ClimbSurface::None;
                pounce->actorsHit.clear();
            }
            return attack;
        }
    }

    Actor composeActor(
        const ActorDefinition& definition,
        const AnimationCatalog& animations,
        int textureId,
        glm::vec2 spawnFeet,
        std::optional<Patrol> patrol,
        const MachineCatalog& machines)
    {
        Actor actor;
        actor.body.bounds = boxStandingOn(spawnFeet, definition.bodySize);
        actor.team = definition.team;
        actor.facing = definition.facing;
        if (const auto* platformer = std::get_if<PlatformerMovementConfig>(&definition.movement))
        {
            validatePlatformerMovementConfig(*platformer);
            actor.platformerMovement = PlatformerMovement{};
            actor.platformerMovement->config = *platformer;
            actor.platformerMovement->grounded = true;
        }
        else if (const auto* flying = std::get_if<FlyingMovementConfig>(&definition.movement))
        {
            actor.flyingMovement = FlyingMovement{*flying};
        }
        if (definition.surfaceClimb)
        {
            actor.surfaceClimb = SurfaceClimb{*definition.surfaceClimb};
        }
        if (definition.health)
        {
            actor.health = Health{*definition.health, *definition.health};
        }
        if (definition.inventorySlots)
        {
            if (*definition.inventorySlots <= 0)
            {
                throw std::invalid_argument("inventorySlots must be positive");
            }
            actor.inventory = Inventory{static_cast<std::size_t>(*definition.inventorySlots)};
        }
        if (definition.senses)
        {
            if (definition.machine.empty())
            {
                throw std::invalid_argument("An NPC with senses requires a state machine");
            }
            actor.brain = NpcBrain{};
            actor.perception = NpcPerception{};
            actor.senses = definition.senses;
            actor.pathFollower = PathFollower{};
            actor.machine = startNpcMachine(npcStateMachine(machines, definition.machine));
        }
        else if (!definition.machine.empty())
        {
            throw std::invalid_argument("A state machine requires senses");
        }
        actor.patrol = patrol;
        actor.primaryAttack = freshAttack(definition.primaryAttack);
        actor.secondaryAttack = freshAttack(definition.secondaryAttack);
        if (RangedWeapon* weapon = findAttack<RangedWeapon>(actor))
        {
            weapon->projectileSprite.textureId = textureId;
        }
        if (!definition.animations.empty())
        {
            Animator animator;
            animator.animationSet = animationSet(animations, definition.animations);
            validateAnimationSet(animator.animationSet);
            const auto& frame = clipFor(animator.animationSet, AnimationName::Idle).frames.front();
            actor.sprite = Sprite{textureId, frame};
            actor.sprite->anchor = definition.spriteAnchor;
            actor.animator = std::move(animator);
        }
        validateActor(actor);
        return actor;
    }

    void validateActorDefinition(
        const ActorDefinition& definition,
        const AnimationCatalog& animations,
        const MachineCatalog& machines)
    {
        composeActor(definition, animations, 0, {}, std::nullopt, machines);
    }
}
