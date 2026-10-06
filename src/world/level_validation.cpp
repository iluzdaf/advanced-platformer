#include "advanced_platformer/world/level_validation.hpp"

#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/actor_navigation.hpp"
#include "advanced_platformer/navigation/navigation_path.hpp"
#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/world/level_exit.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"

namespace advanced_platformer
{
    namespace
    {
        bool hasClearance(const TileMap& map, const Aabb& bounds)
        {
            const CellRange cells = cellsCovered(map.tileSize(), bounds);
            for (int row = cells.first.y; row <= cells.last.y; ++row)
            {
                for (int column = cells.first.x; column <= cells.last.x; ++column)
                {
                    if (map.blocksMovement({column, row}))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        bool hasGroundSupport(const TileMap& map, const Aabb& bounds)
        {
            const CellRange cells = cellsCovered(map.tileSize(), bounds);
            const int rowBelow =
                cellAt(map.tileSize(), {bounds.topLeft.x, bottomOf(bounds) + EdgeTolerance}).y;

            for (int column = cells.first.x; column <= cells.last.x; ++column)
            {
                if (map.blocksMovement({column, rowBelow}))
                {
                    return true;
                }
            }
            return false;
        }

        std::string actorLocation(int level, ActorId actor, std::string_view place)
        {
            return std::format("Level {} actor {} {}", level, actor.value, place);
        }

        void validatePlacement(
            const TileMap& map,
            const Actor& actor,
            const Aabb& bounds,
            int level,
            std::string_view place,
            bool needsGround)
        {
            const std::string location = actorLocation(level, actor.id, place);
            if (!hasClearance(map, bounds))
            {
                throw std::invalid_argument(std::format("{} overlaps a blocked tile", location));
            }
            if (needsGround && !hasGroundSupport(map, bounds))
            {
                throw std::invalid_argument(std::format("{} has no ground support", location));
            }
        }

        void validateAtFeet(
            const TileMap& map,
            const Actor& actor,
            glm::vec2 feet,
            int level,
            std::string_view place,
            bool needsGround)
        {
            const Aabb bounds = boxStandingOn(feet, actor.body.bounds.size);
            validatePlacement(map, actor, bounds, level, place, needsGround);
        }

        // A climber can patrol to a wall or ceiling; navigation takes it to the nearest
        // place it can hold.
        bool patrolNeedsGround(const Actor& actor)
        {
            return actor.platformerMovement.has_value() && !actor.surfaceClimb.has_value();
        }
    }

    void validateLevelActors(const TileMap& map, const World& world, int level)
    {
        for (const Actor& actor : world.actors())
        {
            const bool platformer = actor.platformerMovement.has_value();
            validatePlacement(map, actor, actor.body.bounds, level, "spawn", platformer);
            if (!actor.patrol.has_value())
            {
                continue;
            }
            const bool needsGround = patrolNeedsGround(actor);
            validateAtFeet(
                map, actor, actor.patrol->firstFeet, level, "first patrol point", needsGround);
            validateAtFeet(
                map, actor, actor.patrol->secondFeet, level, "second patrol point", needsGround);
        }

        const Actor* player = world.findActor(world.playerId());
        if (player != nullptr)
        {
            validateAtFeet(
                map,
                *player,
                world.playerSpawnFeet(),
                level,
                "respawn",
                player->platformerMovement.has_value());
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
        PlatformerConnectionCache cache;
        const std::optional<NavigationPathResult> result =
            findActorPathFillingCache(map, atRespawn, feetOf(exit->bounds), stepSeconds, cache);
        return result.has_value() && result->status == NavigationPathStatus::Found;
    }
}
