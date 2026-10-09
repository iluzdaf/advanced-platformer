#include "advanced_platformer/npc/npc_activity_runner.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/inventory/item.hpp"
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
#include "advanced_platformer/world/level_exit.hpp"
#include "advanced_platformer/world/pickup.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "advanced_platformer/world/world_requests.hpp"

namespace advanced_platformer
{
    namespace
    {
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

        std::optional<glm::vec2> breakableTileBelow(const TileMap& map, glm::vec2 feet)
        {
            const Cell above = cellAtFeet(map.tileSize(), feet);
            const Cell below{above.x, above.y + 1};
            if (!map.contains(below) || !map.blocksMovement(below) ||
                !map.definitionAt(below).breaksIntoTileId.has_value())
            {
                return std::nullopt;
            }
            const float half = static_cast<float>(map.tileSize()) / 2.0F;
            return cellCorner(map.tileSize(), below) + glm::vec2{half, half};
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
            snapshot.health = actor.health;
            snapshot.patrol = actor.patrol;
            if (actor.platformerMovement.has_value())
            {
                snapshot.footing = footingOf(update.map, actor);
            }
            snapshot.facts = facts;
            snapshot.routeStatus = follower.routeStatus;
            snapshot.routeComplete = pathComplete(follower);
            if (facts.targetKnown)
            {
                snapshot.targetFeet = brain.lastKnownTargetFeet;
            }
            snapshot.lastKnownTargetFeet = brain.lastKnownTargetFeet;
            if (target != nullptr)
            {
                snapshot.targetCenter = centerOf(target->body.bounds);
                snapshot.targetKind = target->definitionName;
            }
            if (const std::optional<LevelExit>& exit = update.world.exit(); exit.has_value())
            {
                snapshot.exitFeet = feetOf(exit->bounds);
            }
            for (const Pickup& pickup : update.world.pickups())
            {
                const glm::vec2 feet = feetOf(pickup.body.bounds);
                snapshot.pickups.push_back(
                    NpcPickupSnapshot{
                        .feet = feet,
                        .item = update.world.itemDefinition(pickup.stack.item).name,
                        .quantity = pickup.stack.quantity,
                        .breakableBelow = breakableTileBelow(update.map, feet)});
            }
            return snapshot;
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
        update.scripts.enter(
            actor.id, activity, activitySnapshot(update, actor, brain, follower, target, facts));
    }

    namespace
    {
        void aimToward(Actor& actor, glm::vec2 target)
        {
            actor.intentions.aimDirection = target - centerOf(actor.body.bounds);
        }

        std::optional<std::size_t> slotHolding(
            const World& world,
            const Actor& actor,
            const std::string& item)
        {
            if (!actor.inventory.has_value())
            {
                return std::nullopt;
            }
            const auto& slots = actor.inventory->slots();
            for (std::size_t slot = 0; slot < slots.size(); ++slot)
            {
                const std::optional<ItemStack>& stack = slots[slot];
                if (stack.has_value() && world.itemDefinition(stack->item).name == item)
                {
                    return slot;
                }
            }
            return std::nullopt;
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
            if (command.useItem.has_value())
            {
                if (const std::optional<std::size_t> slot =
                        slotHolding(update.world, actor, *command.useItem);
                    slot.has_value())
                {
                    update.requests.useItem(actor.id, *slot);
                }
            }
        }
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
        const NpcActivityCommand command = update.scripts.update(
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
        update.scripts.exit(
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
