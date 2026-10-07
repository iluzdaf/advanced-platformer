#include "advanced_platformer/npc/npc_facts.hpp"

#include <variant>

#include <glm/geometric.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/combat/attack_system.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/combat/attack.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/movement/pounce.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_senses.hpp"
#include "advanced_platformer/world/tile_map.hpp"

namespace advanced_platformer
{
    namespace
    {
        struct AttackFacts
        {
            bool inRange = false;
            bool ready = false;
            bool active = false;
        };

        AttackFacts attackFacts(const Actor& actor, const Attack& attack, const Actor* target)
        {
            AttackFacts facts;
            if (const auto* bite = std::get_if<BiteAttack>(&attack))
            {
                facts.ready = bite->phase == BitePhase::Ready;
                facts.active = bite->phase == BitePhase::Active;
                facts.inRange =
                    target != nullptr &&
                    overlaps(
                        biteHitbox(actor.body.bounds, *bite, actor.facing), target->body.bounds);
            }
            else if (const auto* weapon = std::get_if<RangedWeapon>(&attack))
            {
                facts.ready = weapon->phase == RangedPhase::Ready;
                facts.active = weapon->phase == RangedPhase::Shoot;
                facts.inRange = target != nullptr;
            }
            else if (const auto* contact = std::get_if<ContactDamage>(&attack))
            {
                facts.ready = true;
                facts.active = contact->active;
                facts.inRange =
                    target != nullptr && overlaps(actor.body.bounds, target->body.bounds);
            }
            else if (const auto* pounce = std::get_if<Pounce>(&attack))
            {
                facts.ready = pounce->phase == PouncePhase::Ready;
                facts.active = pounce->phase == PouncePhase::Airborne;
                facts.inRange =
                    target != nullptr &&
                    glm::distance(centerOf(actor.body.bounds), centerOf(target->body.bounds)) <=
                        pounce->config.range;
            }
            return facts;
        }

        bool targetIsWithinStandoffDistance(const Actor& actor, const NpcBrain& brain)
        {
            const float standoffDistance =
                actor.senses.has_value() ? actor.senses->standoffDistance : 0.0F;
            return glm::distance(feetOf(actor.body.bounds), brain.lastKnownTargetFeet) <
                   standoffDistance;
        }

        bool targetIsOnSameSurface(const TileMap& map, const Actor& actor, const Actor& target)
        {
            if (!actor.platformerMovement.has_value() || !target.platformerMovement.has_value() ||
                !target.platformerMovement->grounded)
            {
                return false;
            }
            const ClimbSurface surface =
                actor.surfaceClimb.has_value() ? actor.surfaceClimb->surface : ClimbSurface::None;
            if (surface == ClimbSurface::None && !actor.platformerMovement->grounded)
            {
                return false;
            }
            if (actor.surfaceClimb.has_value())
            {
                return onSameClimbSurface(map, actor.body.bounds, surface, target.body.bounds);
            }
            return onSameGroundRun(map, actor.body.bounds, target.body.bounds);
        }

        bool targetIsWithinNoticeDistance(const Actor& actor, const Actor& target)
        {
            return actor.senses.has_value() &&
                   glm::distance(feetOf(actor.body.bounds), feetOf(target.body.bounds)) <=
                       actor.senses->noticeDistance;
        }
    }

    NpcFacts gatherNpcFacts(
        const TileMap& map,
        const Actor& actor,
        const NpcBrain& brain,
        const NpcPerception& perception,
        const Actor* target,
        float stateElapsed)
    {
        NpcFacts facts;
        facts.targetKnown = target != nullptr;
        facts.targetVisible = perception.targetVisible;
        const Actor* visibleTarget = perception.targetVisible ? target : nullptr;
        if (actor.primaryAttack.has_value())
        {
            const AttackFacts primary = attackFacts(actor, *actor.primaryAttack, visibleTarget);
            facts.targetInPrimaryRange = primary.inRange;
            facts.primaryReady = primary.ready;
            facts.primaryActive = primary.active;
        }
        if (actor.secondaryAttack.has_value())
        {
            const AttackFacts secondary = attackFacts(actor, *actor.secondaryAttack, visibleTarget);
            facts.targetInSecondaryRange = secondary.inRange;
            facts.secondaryReady = secondary.ready;
            facts.secondaryActive = secondary.active;
        }
        facts.targetWithinStandoffDistance =
            target != nullptr && targetIsWithinStandoffDistance(actor, brain);
        facts.heardLanding = perception.heardLanding;
        facts.targetOnSameSurface = target != nullptr && targetIsOnSameSurface(map, actor, *target);
        facts.targetWithinNoticeDistance =
            target != nullptr && targetIsWithinNoticeDistance(actor, *target);
        facts.movementBlocked =
            actor.platformerMovement.has_value() && actor.platformerMovement->blocked;
        facts.hasPatrol = actor.patrol.has_value();
        const float searchDuration = actor.senses.has_value() ? actor.senses->searchDuration : 0.0F;
        facts.searches = searchDuration > 0.0F;
        facts.searchTimeUp = stateElapsed >= searchDuration;
        facts.stateElapsed = stateElapsed;
        return facts;
    }
}
