#include "advanced_platformer/world/world_requests.hpp"

#include "advanced_platformer/math/validation.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>

#include <glm/vec2.hpp>
#include <vector>

#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/inventory/item_use.hpp"
#include "advanced_platformer/world/world.hpp"

namespace advanced_platformer
{
    void WorldRequests::damage(ActorId target, int amount, std::optional<glm::vec2> knockback)
    {
        if (!isValid(target) || amount <= 0)
        {
            throw std::invalid_argument("Damage requests require a target and positive amount");
        }
        if (knockback.has_value() && !isFinite(*knockback))
        {
            throw std::invalid_argument("A knockback must be finite");
        }
        damageRequests.push_back({target, amount, knockback});
    }

    void WorldRequests::remove(ActorId target)
    {
        if (!isValid(target))
        {
            throw std::invalid_argument("Removal requests require a target");
        }
        removalRequests.push_back(target);
    }

    void WorldRequests::spawnProjectile(Projectile projectile)
    {
        projectileSpawns.push_back(projectile);
    }

    void WorldRequests::removeProjectile(std::size_t index)
    {
        projectileRemovals.push_back(index);
    }

    void WorldRequests::spawnProjectileBurst(ProjectileBurst burst)
    {
        projectileBurstSpawns.push_back(burst);
    }

    void WorldRequests::removeProjectileBurst(std::size_t index)
    {
        projectileBurstRemovals.push_back(index);
    }

    bool WorldRequests::empty() const
    {
        return damageRequests.empty() && removalRequests.empty() && projectileSpawns.empty() &&
               projectileRemovals.empty() && projectileBurstSpawns.empty() &&
               projectileBurstRemovals.empty() && pickupCollections.empty() && itemUses.empty();
    }

    const std::vector<ActorId>& WorldRequests::actorsToRemove() const
    {
        return removalRequests;
    }

    void WorldRequests::collectPickup(std::size_t index)
    {
        pickupCollections.push_back(index);
    }

    void WorldRequests::useItem(ActorId actor, std::size_t slot)
    {
        if (!isValid(actor))
        {
            throw std::invalid_argument("Item use requires an actor");
        }
        itemUses.push_back({actor, slot});
    }

    namespace
    {
        void removeEachHighestFirst(
            std::vector<std::size_t>& indexes,
            const std::function<void(std::size_t)>& remove)
        {
            std::ranges::sort(indexes);
            const auto duplicates = std::ranges::unique(indexes);
            indexes.erase(duplicates.begin(), duplicates.end());
            for (auto index = indexes.rbegin(); index != indexes.rend(); ++index)
            {
                remove(*index);
            }
        }
    }

    void applyWorldRequests(World& world, WorldRequests& requests)
    {
        for (const auto& use : requests.itemUses)
        {
            useItem(world, use.actor, use.slot);
        }
        removeEachHighestFirst(
            requests.pickupCollections,
            [&world](std::size_t index) { world.collectPickup(index); });

        for (const ActorId id : requests.removalRequests)
        {
            world.removeActor(id);
        }

        removeEachHighestFirst(
            requests.projectileRemovals,
            [&world](std::size_t index) { world.removeProjectile(index); });
        for (const Projectile& projectile : requests.projectileSpawns)
        {
            world.addProjectile(projectile);
        }

        removeEachHighestFirst(
            requests.projectileBurstRemovals,
            [&world](std::size_t index) { world.removeProjectileBurst(index); });
        for (const ProjectileBurst& burst : requests.projectileBurstSpawns)
        {
            world.addProjectileBurst(burst);
        }

        requests.itemUses.clear();
        requests.pickupCollections.clear();
        requests.removalRequests.clear();
        requests.projectileSpawns.clear();
        requests.projectileRemovals.clear();
        requests.projectileBurstSpawns.clear();
        requests.projectileBurstRemovals.clear();
    }
}
