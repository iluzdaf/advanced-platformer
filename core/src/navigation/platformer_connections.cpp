#include "advanced_platformer/navigation/platformer_connections.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include <glm/common.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/input/input_program.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/navigation/route.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/navigation/platformer_cells.hpp"
#include "advanced_platformer/navigation/platformer_traversal_profile.hpp"
#include "advanced_platformer/navigation/traversal.hpp"
#include "advanced_platformer/physics/body.hpp"
#include "advanced_platformer/physics/collision.hpp"
#include "advanced_platformer/world/tile_map.hpp"

namespace advanced_platformer
{
    namespace
    {
        struct BuiltPlatformerConnections
        {
            std::vector<RouteConnection> connections;
            CellRange footprint;
            std::vector<WalkSimulationResult> walksToCache;
            int simulatedTicks = 0;
        };

        constexpr int MaximumConnectionSimulationTicks = 120;
        constexpr int MaximumClimbSimulationTicks = 240;
        constexpr float ClimbArrivalDistance = 0.02F;
        constexpr float FloorArrivalDistance = 1.0F;
        constexpr float SettledSpeed = 0.02F;

        void recordSimulationInput(
            InputProgram& program,
            const InputIntentions& intentions,
            float stepSeconds)
        {
            if (!program.empty() && program.back().intentions == intentions)
            {
                program.back().duration += stepSeconds;
                return;
            }
            program.push_back({stepSeconds, intentions});
        }

        void includeCellsAroundBounds(CellRange& accumulatedCells, int tileSize, const Aabb& bounds)
        {
            const glm::vec2 margin{static_cast<float>(tileSize), static_cast<float>(tileSize)};
            const Aabb around{bounds.topLeft - margin, bounds.size + 2.0F * margin};
            accumulatedCells = unionOf(accumulatedCells, cellsCovered(tileSize, around));
        }

        WalkSimulationResult simulateWalk(
            const TileMap& map,
            Cell start,
            Cell destinationCell,
            const PlatformerTraversalProfile& profile)
        {
            const int tileSize = map.tileSize();
            Body body{boxInCell(tileSize, start, profile.size), {0.0F, 0.0F}};
            PlatformerMovement movement{profile.movement, true, 0.0F, 0.0F};
            PathFollower follower;
            setPath(
                follower,
                {feetInCell(tileSize, start),
                 {{feetInCell(tileSize, destinationCell), Traversal::Walk, {}}}});

            WalkSimulationResult walk{
                destinationCell.x - start.x, std::nullopt, cellsCovered(tileSize, body.bounds), 0};
            for (int tick = 0; tick < MaximumConnectionSimulationTicks; ++tick)
            {
                const InputIntentions intentions =
                    followPlatformerPath(body, movement, follower, profile.stepSeconds);
                if (pathComplete(follower))
                {
                    walk.cost = tick;
                    break;
                }
                updatePlatformerMovement(map, body, movement, intentions, profile.stepSeconds);
                includeCellsAroundBounds(walk.sweep, tileSize, body.bounds);
                ++walk.simulatedTicks;
            }
            walk.sweep = {
                {walk.sweep.first.x - start.x, walk.sweep.first.y - start.y},
                {walk.sweep.last.x - start.x, walk.sweep.last.y - start.y}};
            return walk;
        }

        bool touchesHorizontalMapEdge(const TileMap& map, const Aabb& bounds, float direction)
        {
            return (direction < 0.0F && bounds.topLeft.x <= EdgeTolerance) ||
                   (direction > 0.0F && rightOf(bounds) >= map.pixelWidth() - EdgeTolerance);
        }

        struct TraversalAttempt
        {
            Traversal traversal;
            int direction;
            int jumpHoldTicks = 0;
        };

        InputIntentions makeTraversalIntentions(
            Traversal traversal,
            float direction,
            int tick,
            int jumpHoldTicks,
            bool hasLanded)
        {
            InputIntentions intentions;
            intentions.direction.x = hasLanded ? 0.0F : direction;
            if (traversal == Traversal::Jump && !hasLanded)
            {
                intentions.jumpPressed = tick == 0;
                intentions.jumpHeld = tick < jumpHoldTicks;
            }
            return intentions;
        }

        std::optional<Cell> tryFindLandingCell(
            const TileMap& map,
            Cell start,
            const Aabb& bounds,
            glm::vec2 bodySize)
        {
            const Cell destinationCell = cellAtFeet(map.tileSize(), feetOf(bounds));
            if (destinationCell == start || !canStandAt(map, destinationCell, bodySize))
            {
                return std::nullopt;
            }
            return destinationCell;
        }

        struct AirborneSimulationResult
        {
            std::optional<Cell> landingCell;
            int simulatedTicks = 0;
            InputProgram inputs;
            CellRange footprint;
        };

        AirborneSimulationResult simulateAirborneTraversal(
            const TileMap& map,
            Cell start,
            const PlatformerTraversalProfile& profile,
            const TraversalAttempt& attempt)
        {
            Body body{boxInCell(map.tileSize(), start, profile.size), {0.0F, 0.0F}};
            PlatformerMovement movement{profile.movement, true, 0.0F, 0.0F};
            AirborneSimulationResult result{
                std::nullopt, 0, {}, cellsCovered(map.tileSize(), body.bounds)};
            bool leftGround = false;
            std::optional<Cell> landing;
            const float direction = static_cast<float>(attempt.direction);

            for (int tick = 0; tick < MaximumConnectionSimulationTicks; ++tick)
            {
                if (touchesHorizontalMapEdge(map, body.bounds, direction))
                {
                    return result;
                }

                const InputIntentions intentions = makeTraversalIntentions(
                    attempt.traversal, direction, tick, attempt.jumpHoldTicks, landing.has_value());
                recordSimulationInput(result.inputs, intentions, profile.stepSeconds);
                updatePlatformerMovement(map, body, movement, intentions, profile.stepSeconds);
                includeCellsAroundBounds(result.footprint, map.tileSize(), body.bounds);
                ++result.simulatedTicks;

                leftGround = leftGround || !movement.grounded;
                if (!leftGround || !movement.grounded)
                {
                    continue;
                }

                if (!landing.has_value())
                {
                    landing = tryFindLandingCell(map, start, body.bounds, profile.size);
                    if (!landing.has_value())
                    {
                        return result;
                    }
                }
                if (body.velocity.x != 0.0F)
                {
                    continue;
                }
                const Cell stoppedCell = cellAtFeet(map.tileSize(), feetOf(body.bounds));
                if (stoppedCell != landing.value())
                {
                    return result;
                }
                result.landingCell = stoppedCell;
                return result;
            }
            return result;
        }

        void keepCheapest(std::vector<RouteConnection>& connections, RouteConnection candidate)
        {
            const auto existing = std::ranges::find_if(
                connections,
                [&candidate](const RouteConnection& connection)
                {
                    return connection.step.destination.cell == candidate.step.destination.cell &&
                           connection.step.traversal == candidate.step.traversal;
                });
            if (existing == connections.end())
            {
                connections.push_back(std::move(candidate));
            }
            else if (candidate.cost < existing->cost)
            {
                *existing = std::move(candidate);
            }
        }

        struct ConnectionPlan
        {
            std::vector<TraversalAttempt> attempts;
            CellRange footprint;
        };

        ConnectionPlan planPlatformerConnections(const TileMap& map, Cell start, glm::vec2 bodySize)
        {
            const int tileSize = map.tileSize();
            ConnectionPlan plan{{}, cellsCovered(tileSize, boxInCell(tileSize, start, bodySize))};
            const auto recordProbe = [&](Cell cell)
            {
                includeCellsAroundBounds(
                    plan.footprint, tileSize, boxInCell(tileSize, cell, bodySize));
            };
            recordProbe(start);
            if (!canStandAt(map, start, bodySize))
            {
                return plan;
            }

            constexpr std::array<int, 2> Directions{-1, 1};
            constexpr std::array<int, 2> JumpHoldTicks{1, MaximumConnectionSimulationTicks};
            plan.attempts.reserve(Directions.size() * (1 + JumpHoldTicks.size()));
            for (const int direction : Directions)
            {
                const Cell adjacent{start.x + direction, start.y};
                recordProbe(adjacent);
                const Traversal ground =
                    canStandAt(map, adjacent, bodySize) ? Traversal::Walk : Traversal::Fall;
                plan.attempts.push_back({ground, direction});
                for (const int holdTicks : JumpHoldTicks)
                {
                    plan.attempts.push_back({Traversal::Jump, direction, holdTicks});
                }
            }
            return plan;
        }

        BuiltPlatformerConnections buildWalkConnections(
            const TileMap& map,
            Cell start,
            const PlatformerTraversalProfile& profile,
            const PlatformerConnectionCache& walkCache,
            int direction)
        {
            const int tileSize = map.tileSize();
            BuiltPlatformerConnections result{
                {}, cellsCovered(tileSize, boxInCell(tileSize, start, profile.size)), {}, 0};
            Cell destination{start.x + direction, start.y};
            do
            {
                const int columns = destination.x - start.x;
                const WalkSimulationResult* cached = walkCache.cachedWalk(columns, profile);
                const WalkSimulationResult walk =
                    cached != nullptr ? *cached : simulateWalk(map, start, destination, profile);
                if (cached == nullptr)
                {
                    result.simulatedTicks += walk.simulatedTicks;
                    result.walksToCache.push_back(walk);
                }
                result.footprint = unionOf(
                    result.footprint,
                    {{start.x + walk.sweep.first.x, start.y + walk.sweep.first.y},
                     {start.x + walk.sweep.last.x, start.y + walk.sweep.last.y}});
                if (!walk.cost.has_value())
                {
                    break;
                }
                result.connections.push_back(
                    {{{destination}, Traversal::Walk, {}}, walk.cost.value()});
                destination.x += direction;
                includeCellsAroundBounds(
                    result.footprint, tileSize, boxInCell(tileSize, destination, profile.size));
            } while (canStandAt(map, destination, profile.size));
            return result;
        }

        BuiltPlatformerConnections buildAirborneConnection(
            const TileMap& map,
            Cell start,
            const PlatformerTraversalProfile& profile,
            const TraversalAttempt& attempt)
        {
            AirborneSimulationResult simulated =
                simulateAirborneTraversal(map, start, profile, attempt);
            BuiltPlatformerConnections result{
                {}, simulated.footprint, {}, simulated.simulatedTicks};
            if (simulated.landingCell.has_value())
            {
                result.connections.push_back(
                    {{{simulated.landingCell.value()},
                      attempt.traversal,
                      std::move(simulated.inputs)},
                     simulated.simulatedTicks});
            }
            return result;
        }

        bool climbArrived(
            const Body& body,
            const PlatformerMovement& movement,
            const SurfaceClimb& climb,
            const Aabb& target,
            ClimbSurface surface)
        {
            if (climb.surface != surface)
            {
                return false;
            }
            const bool toFloor = surface == ClimbSurface::None;
            const float tolerance = toFloor ? FloorArrivalDistance : ClimbArrivalDistance;
            if (std::abs(body.bounds.topLeft.x - target.topLeft.x) > tolerance ||
                std::abs(body.bounds.topLeft.y - target.topLeft.y) > tolerance)
            {
                return false;
            }
            return !toFloor || (movement.grounded && std::abs(body.velocity.x) <= SettledSpeed);
        }

        InputIntentions climbToward(
            const TileMap& map,
            const Body& body,
            RouteLocation from,
            RouteLocation destination,
            const Aabb& target,
            float distancePerTick)
        {
            InputIntentions intentions;
            intentions.climbGrip = ClimbGrip::Hold;
            const glm::vec2 offset = target.topLeft - body.bounds.topLeft;
            if (from.surface == ClimbSurface::None)
            {
                if (!touchesClimbable(map, body.bounds, destination.surface))
                {
                    intentions.direction.x = offset.x / distancePerTick;
                }
                else
                {
                    intentions.direction.x =
                        destination.surface == ClimbSurface::LeftWall ? -1.0F : 1.0F;
                }
            }
            else
            {
                const glm::length_t alongSurface = from.surface == ClimbSurface::Ceiling ? 0 : 1;
                const glm::length_t acrossSurface = 1 - alongSurface;
                if (std::abs(offset[alongSurface]) > ClimbArrivalDistance)
                {
                    intentions.direction[alongSurface] = offset[alongSurface] / distancePerTick;
                }
                else if (std::abs(offset[acrossSurface]) > ClimbArrivalDistance)
                {
                    intentions.direction[acrossSurface] = offset[acrossSurface] / distancePerTick;
                }
                else if (from.surface != destination.surface)
                {
                    if (destination.surface == ClimbSurface::Ceiling)
                    {
                        intentions.direction.x =
                            from.surface == ClimbSurface::LeftWall ? -1.0F : 1.0F;
                    }
                    else
                    {
                        intentions.direction.y = -1.0F;
                    }
                }
            }
            intentions.direction = glm::clamp(intentions.direction, -1.0F, 1.0F);
            return intentions;
        }

        BuiltPlatformerConnections buildClimbConnection(
            const TileMap& map,
            RouteLocation from,
            RouteLocation destination,
            const PlatformerTraversalProfile& profile,
            const SurfaceClimbConfig& climbConfig)
        {
            const int tileSize = map.tileSize();
            const Aabb target = boundsAtSurface(tileSize, destination, profile.size);
            Body body{boundsAtSurface(tileSize, from, profile.size), {0.0F, 0.0F}};
            BuiltPlatformerConnections result{{}, cellsCovered(tileSize, body.bounds), {}, 0};
            includeCellsAroundBounds(result.footprint, tileSize, body.bounds);
            includeCellsAroundBounds(result.footprint, tileSize, target);
            if (!canOccupy(map, destination, profile.size))
            {
                return result;
            }

            PlatformerMovement movement{
                profile.movement, touchingSurfaces(map, body.bounds).ground, 0.0F, 0.0F};
            SurfaceClimb climb{climbConfig, from.surface};
            const bool toFloor = destination.surface == ClimbSurface::None;
            PathFollower walkToFloor;
            if (toFloor)
            {
                setPath(
                    walkToFloor, {feetOf(body.bounds), {{feetOf(target), Traversal::Walk, {}}}});
            }
            InputProgram inputs;
            for (int tick = 0; tick < MaximumClimbSimulationTicks; ++tick)
            {
                if (climbArrived(body, movement, climb, target, destination.surface))
                {
                    if (tick > 0)
                    {
                        result.connections.push_back(
                            {{destination, Traversal::Climb, std::move(inputs)},
                             tick,
                             from.surface});
                    }
                    return result;
                }
                InputIntentions intentions;
                if (toFloor)
                {
                    intentions =
                        followPlatformerPath(body, movement, walkToFloor, profile.stepSeconds);
                    intentions.climbGrip = ClimbGrip::Release;
                }
                else
                {
                    intentions = climbToward(
                        map,
                        body,
                        from,
                        destination,
                        target,
                        climbConfig.speed * profile.stepSeconds);
                }
                inputs.push_back({profile.stepSeconds, intentions});
                updateSurfaceClimbMovement(
                    map, body, movement, climb, intentions, profile.stepSeconds);
                includeCellsAroundBounds(result.footprint, tileSize, body.bounds);
                ++result.simulatedTicks;
                if (from.surface != ClimbSurface::None && !toFloor &&
                    climb.surface == ClimbSurface::None)
                {
                    return result;
                }
            }
            return result;
        }

        BuiltPlatformerConnections buildReleaseFallConnection(
            const TileMap& map,
            RouteLocation from,
            const PlatformerTraversalProfile& profile,
            const SurfaceClimbConfig& climbConfig)
        {
            const int tileSize = map.tileSize();
            Body body{boundsAtSurface(tileSize, from, profile.size), {0.0F, 0.0F}};
            BuiltPlatformerConnections result{{}, cellsCovered(tileSize, body.bounds), {}, 0};
            includeCellsAroundBounds(result.footprint, tileSize, body.bounds);
            PlatformerMovement movement{
                profile.movement, touchingSurfaces(map, body.bounds).ground, 0.0F, 0.0F};
            if (movement.grounded)
            {
                return result;
            }

            SurfaceClimb climb{climbConfig, from.surface};
            InputIntentions release;
            release.climbGrip = ClimbGrip::Release;
            InputProgram inputs;
            for (int tick = 0; tick < MaximumConnectionSimulationTicks; ++tick)
            {
                recordSimulationInput(inputs, release, profile.stepSeconds);
                updateSurfaceClimbMovement(
                    map, body, movement, climb, release, profile.stepSeconds);
                includeCellsAroundBounds(result.footprint, tileSize, body.bounds);
                ++result.simulatedTicks;
                if (!movement.grounded)
                {
                    continue;
                }
                const std::optional<Cell> landing =
                    tryFindLandingCell(map, from.cell, body.bounds, profile.size);
                if (landing.has_value())
                {
                    result.connections.push_back(
                        {{{*landing}, Traversal::Fall, std::move(inputs)},
                         result.simulatedTicks,
                         from.surface});
                }
                return result;
            }
            return result;
        }

        BuiltPlatformerConnections buildClimbConnections(
            const TileMap& map,
            Cell cell,
            const PlatformerTraversalProfile& profile,
            const SurfaceClimbConfig& climbConfig)
        {
            constexpr std::array<ClimbSurface, 4> Surfaces{
                ClimbSurface::None,
                ClimbSurface::LeftWall,
                ClimbSurface::RightWall,
                ClimbSurface::Ceiling};
            const int tileSize = map.tileSize();
            BuiltPlatformerConnections combined{
                {}, cellsCovered(tileSize, boxInCell(tileSize, cell, profile.size)), {}, 0};
            for (const ClimbSurface surface : Surfaces)
            {
                const RouteLocation from{cell, surface};
                includeCellsAroundBounds(
                    combined.footprint, tileSize, boundsAtSurface(tileSize, from, profile.size));
                if (!canOccupy(map, from, profile.size))
                {
                    continue;
                }
                std::vector<BuiltPlatformerConnections> attempts;
                for (const RouteLocation destination : climbDestinationsFrom(from))
                {
                    attempts.push_back(
                        buildClimbConnection(map, from, destination, profile, climbConfig));
                }
                if (surface != ClimbSurface::None)
                {
                    attempts.push_back(buildReleaseFallConnection(map, from, profile, climbConfig));
                }
                for (BuiltPlatformerConnections& attempt : attempts)
                {
                    combined.footprint = unionOf(combined.footprint, attempt.footprint);
                    combined.simulatedTicks += attempt.simulatedTicks;
                    for (RouteConnection& connection : attempt.connections)
                    {
                        combined.connections.push_back(std::move(connection));
                    }
                }
            }
            return combined;
        }

        BuiltPlatformerConnections buildPlatformerConnections(
            const TileMap& map,
            Cell cell,
            const PlatformerTraversalProfile& profile,
            const PlatformerConnectionCache& walkCache)
        {
            requirePositiveSeconds(profile.stepSeconds, "Navigation simulation step");
            const ConnectionPlan plan = planPlatformerConnections(map, cell, profile.size);
            BuiltPlatformerConnections combined{{}, plan.footprint, {}, 0};
            for (const TraversalAttempt& attempt : plan.attempts)
            {
                BuiltPlatformerConnections attemptResult =
                    attempt.traversal == Traversal::Walk
                        ? buildWalkConnections(map, cell, profile, walkCache, attempt.direction)
                        : buildAirborneConnection(map, cell, profile, attempt);
                combined.footprint = unionOf(combined.footprint, attemptResult.footprint);
                combined.simulatedTicks += attemptResult.simulatedTicks;
                for (RouteConnection& connection : attemptResult.connections)
                {
                    keepCheapest(combined.connections, std::move(connection));
                }
                for (const WalkSimulationResult& walk : attemptResult.walksToCache)
                {
                    combined.walksToCache.push_back(walk);
                }
            }
            if (profile.climb.has_value())
            {
                validateSurfaceClimbConfig(*profile.climb);
                BuiltPlatformerConnections climbs =
                    buildClimbConnections(map, cell, profile, *profile.climb);
                combined.footprint = unionOf(combined.footprint, climbs.footprint);
                combined.simulatedTicks += climbs.simulatedTicks;
                for (RouteConnection& connection : climbs.connections)
                {
                    combined.connections.push_back(std::move(connection));
                }
            }
            return combined;
        }
    }

    int cachePlatformerConnections(
        const TileMap& map,
        PlatformerConnectionCache& cache,
        Cell cell,
        const PlatformerTraversalProfile& profile)
    {
        BuiltPlatformerConnections built = buildPlatformerConnections(map, cell, profile, cache);
        for (const WalkSimulationResult& walk : built.walksToCache)
        {
            cache.storeWalk(profile, walk);
        }
        cache.storeConnections(cell, profile, std::move(built.connections), built.footprint);
        return built.simulatedTicks;
    }
}
