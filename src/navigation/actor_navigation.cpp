#include "advanced_platformer/navigation/actor_navigation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/navigation/route.hpp"
#include "advanced_platformer/navigation/navigation_path.hpp"
#include "advanced_platformer/navigation/route_search.hpp"
#include "advanced_platformer/navigation/platformer_cells.hpp"
#include "advanced_platformer/navigation/platformer_traversal_profile.hpp"
#include "advanced_platformer/navigation/traversal.hpp"
#include "advanced_platformer/timing/frame_profile.hpp"
#include "advanced_platformer/world/tile_map.hpp"

namespace advanced_platformer
{
    namespace
    {
        constexpr int JumpStartPenaltyTicks = 30;

        constexpr std::array<ClimbSurface, 4> Surfaces{
            ClimbSurface::None,
            ClimbSurface::LeftWall,
            ClimbSurface::RightWall,
            ClimbSurface::Ceiling};

        constexpr float RestingTolerance = 1.0F;

        std::optional<RouteLocation> restingLocationOf(
            const TileMap& map,
            const Aabb& bounds,
            const PlatformerTraversalProfile& profile)
        {
            if (!isFinite(bounds.topLeft))
            {
                throw std::invalid_argument("A resting body must have a finite position");
            }
            if (!isFinitePositive(bounds.size))
            {
                throw std::invalid_argument("Navigation body size must be finite and positive");
            }
            const int tileSize = map.tileSize();
            const CellRange covered = cellsCovered(tileSize, bounds);
            std::optional<RouteLocation> resting;
            float restingOffset = static_cast<float>(tileSize);
            for (int row = covered.first.y - 1; row <= covered.last.y + 1; ++row)
            {
                for (int column = covered.first.x - 1; column <= covered.last.x + 1; ++column)
                {
                    for (const ClimbSurface surface : Surfaces)
                    {
                        const RouteLocation candidate{{column, row}, surface};
                        if ((surface != ClimbSurface::None && !profile.climb.has_value()) ||
                            !canOccupy(map, candidate, bounds.size))
                        {
                            continue;
                        }
                        const glm::vec2 offset =
                            bounds.topLeft -
                            boundsAtSurface(tileSize, candidate, bounds.size).topLeft;
                        const bool onWall =
                            surface == ClimbSurface::LeftWall || surface == ClimbSurface::RightWall;
                        const float across = std::abs(onWall ? offset.x : offset.y);
                        const float along = std::abs(onWall ? offset.y : offset.x);
                        if (across <= RestingTolerance && along < restingOffset)
                        {
                            resting = candidate;
                            restingOffset = along;
                        }
                    }
                }
            }
            return resting;
        }

        NavigationPath waypointsOf(int tileSize, const Route& route, glm::vec2 bodySize)
        {
            NavigationPath path{feetOf(boundsAtSurface(tileSize, route.start, bodySize)), {}};
            path.waypoints.reserve(route.steps.size());
            for (const RouteStep& step : route.steps)
            {
                path.waypoints.push_back(
                    {feetOf(boundsAtSurface(tileSize, step.destination, bodySize)),
                     step.traversal,
                     step.inputs});
            }
            return path;
        }

        NavigationPathResult pathResultOf(
            int tileSize,
            const Route& route,
            glm::vec2 bodySize,
            Cell goal,
            glm::vec2 goalFeet)
        {
            NavigationPath path = waypointsOf(tileSize, route, bodySize);
            const float remaining = glm::distance(endOf(path), goalFeet);
            const NavigationPathStatus status = endOf(route).cell == goal
                                                    ? NavigationPathStatus::Found
                                                    : NavigationPathStatus::Unreachable;
            return {status, std::move(path), remaining};
        }

        int manhattanHeuristic(Cell cell, Cell goal)
        {
            return std::abs(cell.x - goal.x) + std::abs(cell.y - goal.y);
        }

        void flyingConnections(
            const TileMap& map,
            Cell cell,
            std::vector<RouteConnection>& connections)
        {
            constexpr std::array<glm::ivec2, 4> Directions{
                glm::ivec2{-1, 0}, glm::ivec2{1, 0}, glm::ivec2{0, -1}, glm::ivec2{0, 1}};

            connections.clear();
            for (const glm::ivec2 direction : Directions)
            {
                const Cell candidate{cell.x + direction.x, cell.y + direction.y};
                if (map.contains(candidate) && !map.blocksMovement(candidate))
                {
                    connections.push_back({{{candidate}, Traversal::Fly, {}}, 1});
                }
            }
        }

        std::optional<NavigationPathResult> findFlyingPath(
            const TileMap& map,
            const Aabb& body,
            glm::vec2 goalFeet,
            FrameProfile* profile)
        {
            requireFinite(goalFeet, "A navigation goal");
            if (!isFinite(body.topLeft) || !isFinitePositive(body.size))
            {
                throw std::invalid_argument("A flying body must be finite and positive-sized");
            }
            addFrameStatistic(profile, "Navigation", "Path searches");
            const int tileSize = map.tileSize();
            const Cell start = cellAtFeet(tileSize, feetOf(body));
            if (!map.contains(start))
            {
                return std::nullopt;
            }
            const Cell goal = cellAtFeet(tileSize, goalFeet);
            int cellsExpanded = 0;
            std::vector<RouteConnection> leaving;
            const ConnectionFunction connections =
                [&map, profile, &cellsExpanded, &leaving](
                    RouteLocation location) -> std::optional<std::span<const RouteConnection>>
            {
                const PhaseScope connectionPhase(profile, "Navigation", "Connection retrieval");
                ++cellsExpanded;
                flyingConnections(map, location.cell, leaving);
                return std::span<const RouteConnection>(leaving);
            };
            RouteSearchResult result;
            {
                const PhaseScope algorithmPhase(profile, "Navigation", "Search algorithm");
                result =
                    findLowestCostRoute({start}, goal, map.size(), connections, manhattanHeuristic);
            }
            addFrameStatistic(profile, "Navigation", "Cells expanded", cellsExpanded);
            if (!result.route.has_value())
            {
                throw std::logic_error("A completed route search returned no route");
            }
            return pathResultOf(tileSize, *result.route, body.size, goal, goalFeet);
        }

        int platformerTickHeuristic(
            int tileSize,
            Cell cell,
            Cell goal,
            const PlatformerTraversalProfile& profile)
        {
            requirePositiveSeconds(profile.stepSeconds, "Navigation simulation step");
            if (!isFiniteNonNegative(profile.movement.maximumSpeed))
            {
                throw std::invalid_argument(
                    "Platformer navigation maximum speed must be finite and non-negative");
            }
            float maximumSpeed = profile.movement.maximumSpeed;
            if (profile.climb.has_value())
            {
                validateSurfaceClimbConfig(*profile.climb);
                maximumSpeed = std::max(maximumSpeed, profile.climb->speed);
            }

            const int columnsBetween = std::abs(goal.x - cell.x) - 1;
            if (columnsBetween <= 0 || maximumSpeed == 0.0F)
            {
                return 0;
            }
            const float distance = static_cast<float>(columnsBetween * tileSize);
            return static_cast<int>(std::ceil(distance / (maximumSpeed * profile.stepSeconds)));
        }

        int withJumpStartPenalty(const RouteConnection& connection)
        {
            if (connection.step.traversal != Traversal::Jump)
            {
                return connection.cost;
            }
            if (connection.cost > std::numeric_limits<int>::max() - JumpStartPenaltyTicks)
            {
                throw std::overflow_error("A route connection cost is too large");
            }
            return connection.cost + JumpStartPenaltyTicks;
        }

        void requireValid(glm::vec2 goalFeet, const PlatformerTraversalProfile& profile)
        {
            requirePositiveSeconds(profile.stepSeconds, "Navigation simulation step");
            requireFinite(goalFeet, "A navigation goal");
            if (profile.climb.has_value())
            {
                validateSurfaceClimbConfig(*profile.climb);
            }
        }

        std::optional<NavigationPathResult> findPlatformerPath(
            const TileMap& map,
            const Aabb& body,
            glm::vec2 goalFeet,
            const PlatformerTraversalProfile& profile,
            PlatformerConnectionCache& cache,
            FrameProfile* frameProfile)
        {
            requireValid(goalFeet, profile);

            addFrameStatistic(frameProfile, "Navigation", "Path searches");

            const std::optional<RouteLocation> resting = restingLocationOf(map, body, profile);
            if (!resting.has_value())
            {
                return std::nullopt;
            }

            const RouteLocation start = *resting;
            const int tileSize = map.tileSize();
            const Cell goal = cellAtFeet(tileSize, goalFeet);
            {
                const PhaseScope cachePhase(frameProfile, "Navigation", "Path cache");
                cache.applyRecordedTileBreaks(map, frameProfile);
            }

            int cellsExpanded = 0;
            const ConnectionFunction connections =
                [&profile, &cache, frameProfile, &cellsExpanded](
                    RouteLocation location) -> std::optional<std::span<const RouteConnection>>
            {
                const PhaseScope connectionPhase(
                    frameProfile, "Navigation", "Connection retrieval");

                const std::vector<RouteConnection>* cellConnections =
                    cache.cachedConnections(location.cell, profile);
                if (cellConnections == nullptr)
                {
                    return std::nullopt;
                }
                ++cellsExpanded;

                const auto [first, last] = std::ranges::equal_range(
                    *cellConnections, location.surface, {}, &RouteConnection::sourceSurface);
                return std::span<const RouteConnection>(first, last);
            };

            const HeuristicFunction heuristic = [tileSize, &profile](Cell cell, Cell goalCell)
            { return platformerTickHeuristic(tileSize, cell, goalCell, profile); };

            RouteSearchResult result;
            {
                const PhaseScope algorithmPhase(frameProfile, "Navigation", "Search algorithm");
                result = findLowestCostRoute(
                    start, goal, map.size(), connections, heuristic, withJumpStartPenalty);
            }

            addFrameStatistic(frameProfile, "Navigation", "Cells expanded", cellsExpanded);

            if (result.unexpandedLocation.has_value())
            {
                {
                    const PhaseScope cachePhase(frameProfile, "Navigation", "Path cache");
                    cache.queue(result.unexpandedLocation->cell, profile);
                    cache.prioritise(result.unexpandedLocation->cell, profile);
                }
                addFrameStatistic(frameProfile, "Navigation", "Paths deferred");
                return NavigationPathResult{NavigationPathStatus::Deferred, std::nullopt};
            }

            if (!result.route.has_value())
            {
                throw std::logic_error("A completed route search returned no route");
            }
            return pathResultOf(tileSize, *result.route, profile.size, goal, goalFeet);
        }
    }

    PlatformerTraversalProfile platformerTraversalProfileFor(const Actor& actor, float stepSeconds)
    {
        if (!actor.platformerMovement.has_value())
        {
            throw std::invalid_argument(
                "A platformer traversal profile requires platformer movement");
        }
        return {
            .size = actor.body.bounds.size,
            .movement = actor.platformerMovement->config,
            .stepSeconds = stepSeconds,
            .climb = actor.surfaceClimb.has_value()
                         ? std::optional<SurfaceClimbConfig>{actor.surfaceClimb->config}
                         : std::nullopt};
    }

    std::optional<NavigationPathResult> findActorPath(
        const TileMap& map,
        const Actor& actor,
        glm::vec2 goalFeet,
        float stepSeconds,
        PlatformerConnectionCache& cache,
        FrameProfile* frameProfile)
    {
        if (actor.flyingMovement.has_value())
        {
            return findFlyingPath(map, actor.body.bounds, goalFeet, frameProfile);
        }
        if (!actor.platformerMovement.has_value())
        {
            return std::nullopt;
        }
        return findPlatformerPath(
            map,
            actor.body.bounds,
            goalFeet,
            platformerTraversalProfileFor(actor, stepSeconds),
            cache,
            frameProfile);
    }
}
