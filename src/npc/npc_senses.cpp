#include "advanced_platformer/npc/npc_senses.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/platformer_cells.hpp"
#include "advanced_platformer/navigation/route.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/world/sight.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"

namespace advanced_platformer
{
    namespace
    {
        struct SensingNpc
        {
            Actor& actor;
            NpcBrain& brain;
            NpcPerception& perception;
            const NpcSenses& senses;
        };

        bool withinNoticeDistance(const Aabb& observer, const Aabb& target, const NpcSenses& senses)
        {
            if (!isFiniteNonNegative(senses.noticeDistance))
            {
                throw std::invalid_argument("NPC notice distance must be finite and non-negative");
            }

            const glm::vec2 offset = centerOf(target) - centerOf(observer);
            return glm::dot(offset, offset) <= senses.noticeDistance * senses.noticeDistance;
        }

        bool canSeeTarget(
            const TileMap& map,
            const Aabb& observer,
            const Aabb& target,
            const NpcSenses& senses)
        {
            if (!withinNoticeDistance(observer, target, senses))
            {
                return false;
            }

            return lineOfSight(map, centerOf(observer), centerOf(target));
        }

        void rememberTarget(NpcBrain& brain, const Actor& target, const NpcSenses& senses)
        {
            brain.target = target.id;
            brain.lastKnownTargetFeet = feetOf(target.body.bounds);
            brain.targetMemoryRemaining = senses.targetMemoryDuration;
        }

        bool hearNoises(
            const TileMap& map,
            SensingNpc& npc,
            const Actor& target,
            const std::vector<WorldEvent>& noises)
        {
            bool heardNoise = false;
            for (const WorldEvent& noise : noises)
            {
                if (noise.actor != target.id)
                {
                    continue;
                }
                const Aabb noiseBounds = boxStandingOn(noise.feet, target.body.bounds.size);
                if (!withinNoticeDistance(npc.actor.body.bounds, noiseBounds, npc.senses))
                {
                    continue;
                }
                if (noise.kind == WorldEventKind::Landing)
                {
                    if (!npc.actor.platformerMovement.has_value() ||
                        !npc.actor.platformerMovement->grounded ||
                        !onSameGroundRun(map, npc.actor.body.bounds, noiseBounds))
                    {
                        continue;
                    }
                    npc.perception.heardLanding = true;
                }
                heardNoise = true;
                rememberTarget(npc.brain, target, npc.senses);
                npc.brain.lastKnownTargetFeet = noise.feet;
            }
            return heardNoise;
        }

        bool observeTarget(const TileMap& map, SensingNpc& npc, const Actor& target)
        {
            if (!canSeeTarget(map, npc.actor.body.bounds, target.body.bounds, npc.senses))
            {
                return false;
            }

            rememberTarget(npc.brain, target, npc.senses);
            npc.perception.targetVisible = true;
            return true;
        }

        std::vector<RouteLocation> surfaceNeighbours(RouteLocation location)
        {
            std::vector<RouteLocation> neighbours = climbDestinationsFrom(location);
            if (location.surface == ClimbSurface::None)
            {
                neighbours.push_back({{location.cell.x - 1, location.cell.y}, ClimbSurface::None});
                neighbours.push_back({{location.cell.x + 1, location.cell.y}, ClimbSurface::None});
            }
            return neighbours;
        }

        void decayTargetMemory(const World& world, NpcBrain& brain, float deltaTime)
        {
            if (livingTarget(world, brain) == nullptr)
            {
                brain.target.reset();
                brain.targetMemoryRemaining = 0.0F;
                return;
            }

            brain.targetMemoryRemaining = std::max(0.0F, brain.targetMemoryRemaining - deltaTime);
            if (brain.targetMemoryRemaining == 0.0F)
            {
                brain.target.reset();
            }
        }
    }

    bool onSameGroundRun(const TileMap& map, const Aabb& observer, const Aabb& target)
    {
        const Cell first = cellAtFeet(map.tileSize(), feetOf(observer));
        const Cell last = cellAtFeet(map.tileSize(), feetOf(target));
        if (first.y != last.y)
        {
            return false;
        }
        for (int x = std::min(first.x, last.x); x <= std::max(first.x, last.x); ++x)
        {
            if (!canStandAt(map, {x, first.y}, observer.size))
            {
                return false;
            }
        }
        return true;
    }

    bool onSameClimbSurface(
        const TileMap& map,
        const Aabb& climber,
        ClimbSurface surface,
        const Aabb& target)
    {
        const int tileSize = map.tileSize();
        const RouteLocation goal{cellAtFeet(tileSize, feetOf(target)), ClimbSurface::None};
        const RouteLocation start{
            surface == ClimbSurface::Ceiling
                ? cellAt(tileSize, {centerOf(climber).x, climber.topLeft.y})
                : cellAtFeet(tileSize, feetOf(climber)),
            surface};
        if (!map.contains(start.cell))
        {
            return false;
        }

        const auto indexOf = [&map](RouteLocation location)
        {
            const auto cell =
                static_cast<std::size_t>(location.cell.y) * static_cast<std::size_t>(map.width()) +
                static_cast<std::size_t>(location.cell.x);
            return cell * 4U + static_cast<std::size_t>(location.surface);
        };
        std::vector<bool> visited(
            static_cast<std::size_t>(map.width()) * static_cast<std::size_t>(map.height()) * 4U);
        std::vector<RouteLocation> pending{start};
        visited[indexOf(start)] = true;
        while (!pending.empty())
        {
            const RouteLocation location = pending.back();
            pending.pop_back();
            if (location == goal)
            {
                return true;
            }
            for (const RouteLocation next : surfaceNeighbours(location))
            {
                if (!map.contains(next.cell) || visited[indexOf(next)] ||
                    !canOccupy(map, next, climber.size))
                {
                    continue;
                }
                visited[indexOf(next)] = true;
                pending.push_back(next);
            }
        }
        return false;
    }

    const Actor* livingTarget(const World& world, const NpcBrain& brain)
    {
        if (!brain.target.has_value())
        {
            return nullptr;
        }
        const Actor* target = world.findActor(*brain.target);
        return target != nullptr && target->life == LifeState::Alive ? target : nullptr;
    }

    void updateNpcSenses(const TileMap& map, World& world, float deltaTime)
    {
        requireSeconds(deltaTime, "NPC senses time step");

        const Actor* player = world.findActor(world.playerId());
        const std::vector<WorldEvent> noises = world.takeNoises();
        for (Actor& actor : world.actors())
        {
            if (!actor.brain.has_value())
            {
                continue;
            }
            if (!actor.senses.has_value() || !actor.perception.has_value())
            {
                throw std::logic_error("An NPC is missing its senses or perception");
            }

            NpcBrain& brain = *actor.brain;
            NpcPerception& perception = *actor.perception;
            perception = {};
            SensingNpc npc{actor, brain, perception, *actor.senses};
            const bool livingPlayer = player != nullptr && player->life == LifeState::Alive;
            const bool sensesPlayer = actor.life == LifeState::Alive && livingPlayer &&
                                      areOpponents(actor.team, player->team);
            bool heardNoise = false;
            bool sawTarget = false;
            if (sensesPlayer)
            {
                heardNoise = hearNoises(map, npc, *player, noises);
                sawTarget = observeTarget(map, npc, *player);
            }
            if (!heardNoise && !sawTarget)
            {
                decayTargetMemory(world, brain, deltaTime);
            }
        }
    }
}
