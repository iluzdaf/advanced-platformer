#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/combat/combat.hpp"

namespace advanced_platformer
{
    class World;

    class WorldRequests
    {
    public:
        void damage(ActorId target, int amount, std::optional<glm::vec2> knockback = std::nullopt);
        void remove(ActorId target);
        void spawnProjectile(Projectile projectile);
        void removeProjectile(std::size_t index);
        void spawnProjectileBurst(ProjectileBurst burst);
        void removeProjectileBurst(std::size_t index);
        void collectPickup(std::size_t index);
        void useItem(ActorId actor, std::size_t slot);
        bool empty() const;
        const std::vector<ActorId>& actorsToRemove() const;

    private:
        struct DamageRequest
        {
            ActorId target;
            int amount = 0;
            std::optional<glm::vec2> knockback;
        };

        struct UseItemRequest
        {
            ActorId actor;
            std::size_t slot = 0;
        };

        friend void updateLifeState(World&, WorldRequests&, float, float);
        friend void applyWorldRequests(World&, WorldRequests&);

        std::vector<DamageRequest> damageRequests;
        std::vector<ActorId> removalRequests;
        std::vector<Projectile> projectileSpawns;
        std::vector<std::size_t> projectileRemovals;
        std::vector<ProjectileBurst> projectileBurstSpawns;
        std::vector<std::size_t> projectileBurstRemovals;
        std::vector<std::size_t> pickupCollections;
        std::vector<UseItemRequest> itemUses;
    };

    void applyWorldRequests(World& world, WorldRequests& requests);
}
