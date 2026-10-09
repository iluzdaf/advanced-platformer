#include "level_reload.hpp"

#include "content/item_catalog.hpp"
#include "level_composition.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/combat/attack.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/inventory/inventory.hpp"
#include "advanced_platformer/inventory/item.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/pounce.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/world/level_exit.hpp"
#include "advanced_platformer/world/pickup.hpp"
#include "advanced_platformer/world/world.hpp"

namespace advanced_platformer
{
    namespace
    {
        Inventory carryInventory(
            const Inventory& live,
            std::size_t slotCount,
            const std::map<ItemId, ItemId>& itemIds,
            const World& world)
        {
            Inventory result(slotCount);
            for (const std::optional<ItemStack>& slot : live.slots())
            {
                if (!slot.has_value())
                {
                    continue;
                }
                const auto item = itemIds.find(slot->item);
                if (item != itemIds.end())
                {
                    result.add(world.itemDefinition(item->second), slot->quantity);
                }
            }
            return result;
        }

        void resumeMachine(NpcMachine& rebuilt, const NpcMachine& live)
        {
            const std::string& activeState = activeNpcMachineState(live).name;
            const auto& states = rebuilt.definition.states;
            for (std::size_t index = 0; index < states.size(); ++index)
            {
                if (states[index].name == activeState)
                {
                    rebuilt.active = index;
                    rebuilt.stateElapsed = live.stateElapsed;
                    return;
                }
            }
        }

        const Actor& actorIn(const World& world, ActorId id)
        {
            const Actor* actor = world.findActor(id);
            if (actor == nullptr)
            {
                throw std::logic_error("A reloaded level is missing an actor it placed");
            }
            return *actor;
        }

        void carryAttack(std::optional<Attack>& rebuilt, const std::optional<Attack>& live)
        {
            if (!rebuilt.has_value() || !live.has_value() || rebuilt->index() != live->index())
            {
                return;
            }
            if (auto* bite = std::get_if<BiteAttack>(&*rebuilt))
            {
                const BiteAttack& was = std::get<BiteAttack>(*live);
                bite->phase = was.phase;
                bite->phaseTimeRemaining = was.phaseTimeRemaining;
                bite->actorsHit = was.actorsHit;
            }
            else if (auto* weapon = std::get_if<RangedWeapon>(&*rebuilt))
            {
                const RangedWeapon& was = std::get<RangedWeapon>(*live);
                weapon->phase = was.phase;
                weapon->phaseTimeRemaining = was.phaseTimeRemaining;
                weapon->lastFiredTimeSeconds = was.lastFiredTimeSeconds;
            }
            else if (auto* contact = std::get_if<ContactDamage>(&*rebuilt))
            {
                const ContactDamage& was = std::get<ContactDamage>(*live);
                contact->active = was.active;
                contact->actorsHit = was.actorsHit;
            }
            else if (auto* pounce = std::get_if<Pounce>(&*rebuilt))
            {
                const Pounce& was = std::get<Pounce>(*live);
                pounce->phase = was.phase;
                pounce->phaseTimeRemaining = was.phaseTimeRemaining;
                pounce->launchedFrom = was.launchedFrom;
                pounce->actorsHit = was.actorsHit;
            }
        }

        Actor carryActorState(
            Actor rebuilt,
            const Actor& live,
            const std::map<ItemId, ItemId>& itemIds,
            const World& world)
        {
            rebuilt.id = live.id;
            moveFeetTo(rebuilt.body.bounds, feetOf(live.body.bounds));
            rebuilt.body.velocity = live.body.velocity;
            rebuilt.intentions = live.intentions;
            rebuilt.facing = live.facing;
            rebuilt.life = live.life;
            rebuilt.deathTimeRemaining = live.deathTimeRemaining;
            rebuilt.lastDamageTimeSeconds = live.lastDamageTimeSeconds;
            rebuilt.screenVisibility = live.screenVisibility;

            if (rebuilt.platformerMovement.has_value() && live.platformerMovement.has_value())
            {
                const PlatformerMovementConfig config = rebuilt.platformerMovement->config;
                rebuilt.platformerMovement = live.platformerMovement;
                rebuilt.platformerMovement->config = config;
            }
            if (rebuilt.surfaceClimb.has_value() && live.surfaceClimb.has_value())
            {
                const SurfaceClimbConfig config = rebuilt.surfaceClimb->config;
                rebuilt.surfaceClimb = live.surfaceClimb;
                rebuilt.surfaceClimb->config = config;
            }
            carryAttack(rebuilt.primaryAttack, live.primaryAttack);
            carryAttack(rebuilt.secondaryAttack, live.secondaryAttack);
            if (rebuilt.animator.has_value() && live.animator.has_value())
            {
                rebuilt.animator->current = live.animator->current;
                rebuilt.animator->elapsed = live.animator->elapsed;
            }
            if (rebuilt.health.has_value() && live.health.has_value())
            {
                rebuilt.health->current = std::min(live.health->current, rebuilt.health->maximum);
            }
            if (rebuilt.inventory.has_value() && live.inventory.has_value())
            {
                rebuilt.inventory = carryInventory(
                    *live.inventory, rebuilt.inventory->slots().size(), itemIds, world);
            }
            if (rebuilt.brain.has_value() && live.brain.has_value())
            {
                rebuilt.brain = live.brain;
            }
            if (rebuilt.perception.has_value() && live.perception.has_value())
            {
                rebuilt.perception = live.perception;
            }
            if (rebuilt.machine.has_value() && live.machine.has_value())
            {
                resumeMachine(*rebuilt.machine, *live.machine);
            }
            if (rebuilt.patrol.has_value() && live.patrol.has_value())
            {
                rebuilt.patrol->headingToSecond = live.patrol->headingToSecond;
            }
            return rebuilt;
        }

        void carryPlayer(
            GameLevel& live,
            const GameLevel& fresh,
            const std::map<ItemId, ItemId>& itemIds)
        {
            World& world = live.world;
            const ActorId playerId = world.playerId();
            Actor* player = world.findActor(playerId);
            if (player == nullptr)
            {
                throw std::logic_error("The level to reload has no player");
            }
            const ActorId freshPlayerId = fresh.world.playerId();
            *player = carryActorState(actorIn(fresh.world, freshPlayerId), *player, itemIds, world);
            world.setPlayer(playerId, fresh.playerSpawnFeet);
        }

        void reloadActors(
            GameLevel& live,
            const GameLevel& fresh,
            const std::set<std::string>& placedBefore,
            const std::map<ItemId, ItemId>& itemIds,
            LevelReload& result)
        {
            World& world = live.world;
            std::map<std::string, ActorId> freshActors;
            for (const auto& [actor, placement] : fresh.actorPlacementIds)
            {
                freshActors.emplace(placement, ActorId{actor});
            }

            for (auto entry = live.actorPlacementIds.begin();
                 entry != live.actorPlacementIds.end();)
            {
                const ActorId id{entry->first};
                Actor* actor = world.findActor(id);
                const auto match = freshActors.find(entry->second);
                if (match == freshActors.end())
                {
                    if (actor != nullptr)
                    {
                        world.removeActor(id);
                        result.removed.push_back(entry->second);
                    }
                    entry = live.actorPlacementIds.erase(entry);
                    continue;
                }
                if (actor != nullptr)
                {
                    *actor = carryActorState(
                        actorIn(fresh.world, match->second), *actor, itemIds, world);
                    ++result.kept;
                }
                ++entry;
            }

            for (const auto& [placement, freshId] : freshActors)
            {
                if (placedBefore.contains(placement))
                {
                    continue;
                }
                Actor spawned = actorIn(fresh.world, freshId);
                spawned.id = {};
                const ActorId id = world.addActor(std::move(spawned));
                live.actorPlacementIds[id.value] = placement;
                result.spawned.push_back(placement);
            }
        }

        std::optional<ItemStack> remapStack(
            ItemStack stack,
            const std::map<ItemId, ItemId>& itemIds)
        {
            const auto item = itemIds.find(stack.item);
            if (item == itemIds.end())
            {
                return std::nullopt;
            }
            stack.item = item->second;
            return stack;
        }

        void reloadPickups(
            GameLevel& live,
            const GameLevel& fresh,
            const std::set<std::string>& placedBefore,
            const std::map<ItemId, ItemId>& itemIds,
            LevelReload& result)
        {
            const std::vector<Pickup>& freshPickups = fresh.world.pickups();
            std::map<std::string, std::size_t> freshIndexes;
            for (std::size_t index = 0; index < freshPickups.size(); ++index)
            {
                const std::optional<std::size_t> placement = freshPickups[index].placement;
                if (placement.has_value())
                {
                    freshIndexes.emplace(fresh.pickupPlacementIds.at(*placement), index);
                }
            }

            std::vector<Pickup> pickups;
            for (const Pickup& pickup : live.world.pickups())
            {
                if (!pickup.placement.has_value())
                {
                    const std::optional<ItemStack> stack = remapStack(pickup.stack, itemIds);
                    if (stack.has_value())
                    {
                        Pickup kept = pickup;
                        kept.stack = *stack;
                        pickups.push_back(kept);
                    }
                    continue;
                }
                const std::string& placement = live.pickupPlacementIds.at(*pickup.placement);
                const auto match = freshIndexes.find(placement);
                if (match == freshIndexes.end())
                {
                    result.removed.push_back(placement);
                    continue;
                }
                Pickup rebuilt = freshPickups[match->second];
                moveFeetTo(rebuilt.body.bounds, feetOf(pickup.body.bounds));
                rebuilt.body.velocity = pickup.body.velocity;
                rebuilt.screenVisibility = pickup.screenVisibility;
                pickups.push_back(rebuilt);
                ++result.kept;
            }
            for (const auto& [placement, index] : freshIndexes)
            {
                if (!placedBefore.contains(placement))
                {
                    pickups.push_back(freshPickups[index]);
                    result.spawned.push_back(placement);
                }
            }

            live.world.pickups().clear();
            for (const Pickup& pickup : pickups)
            {
                live.world.addPickup(pickup);
            }
            live.pickupPlacementIds = fresh.pickupPlacementIds;
        }

        void reloadExit(World& world, const World& fresh)
        {
            if (!fresh.exit().has_value())
            {
                return;
            }
            LevelExit exit = *fresh.exit();
            const std::optional<LevelExit>& previous = world.exit();
            if (previous.has_value())
            {
                exit.lastLockedTouchTimeSeconds = previous->lastLockedTouchTimeSeconds;
                exit.openedTimeSeconds = previous->openedTimeSeconds;
            }
            world.setExit(exit);
        }

        std::set<std::string> placementsOf(const GameLevel& level)
        {
            std::set<std::string> placements(
                level.pickupPlacementIds.begin(), level.pickupPlacementIds.end());
            for (const auto& [actor, placement] : level.actorPlacementIds)
            {
                placements.insert(placement);
            }
            return placements;
        }

        std::string listed(const std::vector<std::string>& placements)
        {
            std::string result;
            for (const std::string& placement : placements)
            {
                result += result.empty() ? placement : ", " + placement;
            }
            return result;
        }
    }

    std::map<ItemId, ItemId> matchItemIds(const ItemCatalog& before, const ItemCatalog& after)
    {
        std::map<ItemId, ItemId> result;
        for (const auto& [name, definition] : before.definitions)
        {
            const auto match = after.definitions.find(name);
            if (match != after.definitions.end())
            {
                result.emplace(definition.id, match->second.id);
            }
        }
        return result;
    }

    LevelReload reloadLevel(
        GameLevel& live,
        GameLevel fresh,
        const std::map<ItemId, ItemId>& itemIds)
    {
        LevelReload result;
        const std::set<std::string> placedBefore = live.placedIds;

        for (const Cell cell : live.map.brokenCells())
        {
            fresh.map.breakTile(cell);
        }
        live.map = std::move(fresh.map);
        live.playerSpawnFeet = fresh.playerSpawnFeet;
        live.world.replaceItemDefinitions(fresh.world.items());
        live.world.platformerConnections() = PlatformerConnectionCache{};

        carryPlayer(live, fresh, itemIds);
        reloadActors(live, fresh, placedBefore, itemIds, result);
        reloadPickups(live, fresh, placedBefore, itemIds, result);
        reloadExit(live.world, fresh.world);

        live.placedIds = placementsOf(fresh);
        std::ranges::sort(result.spawned);
        std::ranges::sort(result.removed);
        return result;
    }

    std::string describeReload(const LevelReload& reload)
    {
        std::string result = std::format("Reloaded content: kept {}", reload.kept);
        if (!reload.spawned.empty())
        {
            result += std::format(", spawned {}", listed(reload.spawned));
        }
        if (!reload.removed.empty())
        {
            result += std::format(", removed {}", listed(reload.removed));
        }
        return result;
    }
}
