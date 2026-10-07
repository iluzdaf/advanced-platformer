#include "advanced_platformer/npc/npc_navigation.hpp"

#include <optional>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/navigation/actor_navigation.hpp"
#include "advanced_platformer/navigation/navigation_path.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/npc/npc_update.hpp"
#include "advanced_platformer/timing/frame_profile.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"

namespace advanced_platformer
{
    namespace
    {
        constexpr float ReplanDistance = 8.0F;

        bool needsPath(const PathFollower& follower, glm::vec2 feet, glm::vec2 goal)
        {
            if (!follower.path.has_value() || !follower.goal.has_value())
            {
                return true;
            }
            if (glm::distance(*follower.goal, goal) > ReplanDistance)
            {
                return true;
            }
            return pathComplete(follower) &&
                   glm::distance(feet, endOf(*follower.path)) > ReplanDistance;
        }

        bool plannedBeforeABreak(const TileMap& map, const PathFollower& follower)
        {
            return follower.breaksWhenPlanned != map.brokenCells().size();
        }

        void planPathTo(
            const NpcUpdate& update,
            const Actor& actor,
            PathFollower& follower,
            glm::vec2 goalFeet)
        {
            const TileMap& map = update.map;
            if (!plannedBeforeABreak(map, follower) &&
                !needsPath(follower, feetOf(actor.body.bounds), goalFeet))
            {
                return;
            }

            std::optional<NavigationPathResult> pathResult;
            {
                const PhaseScope searchPhase(update.profile, "Navigation", "Path search");
                pathResult = findActorPath(
                    map,
                    actor,
                    goalFeet,
                    update.deltaTime,
                    update.world.platformerConnections(),
                    update.profile);
            }
            if (!pathResult.has_value())
            {
                return;
            }
            follower.goal = goalFeet;
            follower.breaksWhenPlanned = map.brokenCells().size();
            follower.routeStatus = pathResult->status;
            if (pathResult->status == NavigationPathStatus::Deferred ||
                !pathResult->path.has_value())
            {
                follower.path.reset();
                follower.nextStep = 0;
                follower.programElapsed = 0.0F;
                return;
            }
            setPath(follower, std::move(*pathResult->path));
        }
    }

    InputIntentions intentionsToReach(
        const NpcUpdate& update,
        Actor& actor,
        PathFollower& follower,
        glm::vec2 goalFeet)
    {
        planPathTo(update, actor, follower, goalFeet);
        if (actor.flyingMovement.has_value())
        {
            return followFlyingPath(
                actor.body.bounds, *actor.flyingMovement, follower, update.deltaTime);
        }
        if (actor.platformerMovement.has_value())
        {
            return followPlatformerPath(
                actor.body,
                *actor.platformerMovement,
                follower,
                update.deltaTime,
                actor.surfaceClimb.has_value() ? &*actor.surfaceClimb : nullptr);
        }
        return {};
    }
}
