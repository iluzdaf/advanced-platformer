#include "advanced_platformer/npc/npc_activity_runner.hpp"

#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/navigation/platformer_cells.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_activity_scripts.hpp"
#include "advanced_platformer/npc/npc_facts.hpp"
#include "advanced_platformer/npc/npc_navigation.hpp"
#include "advanced_platformer/npc/npc_update.hpp"
#include "advanced_platformer/world/tile_map.hpp"

namespace advanced_platformer
{
    namespace
    {
        void aimToward(Actor& actor, glm::vec2 targetFeet)
        {
            actor.intentions.aimDirection = targetFeet - feetOf(actor.body.bounds);
        }

        // Whether a walker could stand one body width to each side of where it stands.
        NpcFooting footingOf(const TileMap& map, const Actor& actor)
        {
            const glm::vec2 feet = feetOf(actor.body.bounds);
            const glm::vec2 size = actor.body.bounds.size;
            const auto canStandBeside = [&](float side)
            {
                const Cell beside =
                    cellAtFeet(map.tileSize(), feet + glm::vec2{side * size.x, 0.0F});
                return canStandAt(map, beside, size);
            };
            return {canStandBeside(-1.0F), canStandBeside(1.0F)};
        }

        NpcActivitySnapshot activitySnapshot(
            const NpcUpdate& update,
            const Actor& actor,
            const NpcBrain& brain,
            const PathFollower& follower,
            const Actor* target,
            const NpcFacts& facts)
        {
            NpcActivitySnapshot snapshot;
            snapshot.feet = feetOf(actor.body.bounds);
            snapshot.center = centerOf(actor.body.bounds);
            snapshot.patrol = actor.patrol;
            if (actor.platformerMovement.has_value())
            {
                snapshot.footing = footingOf(update.map, actor);
            }
            snapshot.facts = facts;
            snapshot.hasRoute = follower.path.has_value();
            snapshot.routeComplete = pathComplete(follower);
            if (facts.targetKnown)
            {
                snapshot.targetFeet = brain.lastKnownTargetFeet;
            }
            snapshot.lastKnownTargetFeet = brain.lastKnownTargetFeet;
            if (target != nullptr)
            {
                snapshot.targetCenter = centerOf(target->body.bounds);
            }
            return snapshot;
        }

        NpcActivityScripts& requiredScripts(const NpcUpdate& update)
        {
            if (update.scripts == nullptr)
            {
                throw std::logic_error("An NPC activity needs the scripting runtime");
            }
            return *update.scripts;
        }

        void applyScriptCommand(
            const NpcUpdate& update,
            Actor& actor,
            PathFollower& follower,
            const NpcActivityCommand& command)
        {
            actor.intentions = command.intentions;
            if (command.clearRoute)
            {
                clearPath(follower);
            }
            if (command.routeTo.has_value())
            {
                const InputIntentions movement =
                    intentionsToReach(update, actor, follower, *command.routeTo);
                actor.intentions.direction = movement.direction;
                actor.intentions.jumpPressed = movement.jumpPressed;
                actor.intentions.jumpHeld = movement.jumpHeld;
                actor.intentions.climbGrip = movement.climbGrip;
            }
            if (command.aimAt.has_value())
            {
                aimToward(actor, *command.aimAt);
            }
            if (command.turnPatrol && actor.patrol.has_value())
            {
                actor.patrol->headingToSecond = !actor.patrol->headingToSecond;
            }
        }
    }

    void enterNpcActivity(
        const NpcUpdate& update,
        const Actor& actor,
        const NpcBrain& brain,
        PathFollower& follower,
        const Actor* target,
        const NpcActivity& activity,
        const NpcFacts& facts)
    {
        clearPath(follower);
        requiredScripts(update).enter(
            actor.id, activity, activitySnapshot(update, actor, brain, follower, target, facts));
    }

    void updateNpcActivity(
        const NpcUpdate& update,
        Actor& actor,
        const NpcBrain& brain,
        PathFollower& follower,
        const Actor* target,
        const NpcActivity& activity,
        const NpcFacts& facts)
    {
        const NpcActivityCommand command = requiredScripts(update).update(
            actor.id,
            activity,
            activitySnapshot(update, actor, brain, follower, target, facts),
            update.deltaTime);
        applyScriptCommand(update, actor, follower, command);
    }

    void exitNpcActivity(
        const NpcUpdate& update,
        const Actor& actor,
        const NpcBrain& brain,
        const PathFollower& follower,
        const Actor* target,
        const NpcActivity& activity,
        const NpcFacts& facts)
    {
        requiredScripts(update).exit(
            actor.id, activity, activitySnapshot(update, actor, brain, follower, target, facts));
    }

    void forgetNpcActivities(const std::vector<ActorId>& actors, NpcActivityScripts& scripts)
    {
        for (const ActorId actor : actors)
        {
            scripts.forget(actor);
        }
    }
}
