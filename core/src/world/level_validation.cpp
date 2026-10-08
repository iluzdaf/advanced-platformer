#include "advanced_platformer/world/level_validation.hpp"

#include <cstddef>
#include <format>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/actor_navigation.hpp"
#include "advanced_platformer/navigation/navigation_fill.hpp"
#include "advanced_platformer/navigation/navigation_path.hpp"
#include "advanced_platformer/navigation/platformer_cells.hpp"
#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/physics/collision.hpp"
#include "advanced_platformer/world/level_exit.hpp"
#include "advanced_platformer/world/pickup.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"

namespace advanced_platformer
{
    namespace
    {
        void requireClearance(const TileMap& map, const Aabb& bounds, std::string_view place)
        {
            if (!bodyFits(map, bounds))
            {
                throw std::invalid_argument(std::format("{} overlaps a blocked tile", place));
            }
        }

        void requirePlacement(
            const TileMap& map,
            const Aabb& bounds,
            std::string_view place,
            bool needsGround)
        {
            requireClearance(map, bounds, place);
            if (needsGround && !touchingSurfaces(map, bounds).ground)
            {
                throw std::invalid_argument(std::format("{} has no ground support", place));
            }
        }

        bool patrolNeedsGround(const Actor& actor)
        {
            return actor.platformerMovement.has_value() && !actor.surfaceClimb.has_value();
        }

        void requirePlacementAtFeet(
            const TileMap& map,
            const Actor& actor,
            glm::vec2 feet,
            std::string_view place,
            bool needsGround)
        {
            requirePlacement(map, boxStandingOn(feet, actor.body.bounds.size), place, needsGround);
        }
    }

    void validateActorPlacement(const TileMap& map, const Actor& actor)
    {
        requirePlacement(map, actor.body.bounds, "spawn", actor.platformerMovement.has_value());
        if (!actor.patrol.has_value())
        {
            return;
        }
        const bool needsGround = patrolNeedsGround(actor);
        requirePlacementAtFeet(
            map, actor, actor.patrol->firstFeet, "first patrol point", needsGround);
        requirePlacementAtFeet(
            map, actor, actor.patrol->secondFeet, "second patrol point", needsGround);
    }

    void validatePickupPlacement(const TileMap& map, const Pickup& pickup)
    {
        requireClearance(map, pickup.body.bounds, "spawn");
    }

    bool actorCanReach(
        const TileMap& map,
        const Actor& actor,
        glm::vec2 goalFeet,
        float stepSeconds)
    {
        PlatformerConnectionCache cache;
        std::optional<NavigationPathResult> result =
            findActorPath(map, actor, goalFeet, stepSeconds, cache);
        while (result.has_value() && result->status == NavigationPathStatus::Deferred)
        {
            advanceNavigationFill(map, cache, 1);
            result = findActorPath(map, actor, goalFeet, stepSeconds, cache);
        }
        return result.has_value() && result->status == NavigationPathStatus::Found;
    }

    void validateLevelPlacements(const TileMap& map, const World& world, int level)
    {
        for (const Actor& actor : world.actors())
        {
            try
            {
                validateActorPlacement(map, actor);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(
                    std::format("Level {} actor {} {}", level, actor.id.value, error.what()));
            }
        }

        const std::vector<Pickup>& pickups = world.pickups();
        for (std::size_t index = 0; index < pickups.size(); ++index)
        {
            try
            {
                validatePickupPlacement(map, pickups[index]);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(
                    std::format("Level {} pickup {} {}", level, index, error.what()));
            }
        }

        const Actor* player = world.findActor(world.playerId());
        if (player != nullptr)
        {
            try
            {
                requirePlacementAtFeet(
                    map,
                    *player,
                    world.playerSpawnFeet(),
                    "respawn",
                    player->platformerMovement.has_value());
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(
                    std::format("Level {} actor {} {}", level, player->id.value, error.what()));
            }
        }
    }

    bool playerCanReachExit(const TileMap& map, const World& world, float stepSeconds)
    {
        const Actor* player = world.findActor(world.playerId());
        if (player == nullptr)
        {
            throw std::invalid_argument("A route to the exit needs a player");
        }
        const std::optional<LevelExit>& exit = world.exit();
        if (!exit.has_value())
        {
            throw std::invalid_argument("A route to the exit needs an exit");
        }
        Actor atRespawn = *player;
        moveFeetTo(atRespawn.body.bounds, world.playerSpawnFeet());
        return actorCanReach(map, atRespawn, feetOf(exit->bounds), stepSeconds);
    }
}
