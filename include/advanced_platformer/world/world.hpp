#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/inventory/item.hpp"
#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/world/level_exit.hpp"
#include "advanced_platformer/world/pickup.hpp"

namespace advanced_platformer
{
    enum class NoiseKind
    {
        Landing,
        Shot
    };

    struct NoiseEvent
    {
        ActorId source;
        glm::vec2 feet = {0.0F, 0.0F};
        NoiseKind kind = NoiseKind::Landing;
    };

    class World
    {
    public:
        explicit World(std::vector<ItemDefinition> items = {});
        const ItemDefinition& itemDefinition(ItemId id) const;
        const std::vector<ItemDefinition>& items() const;
        void replaceItemDefinitions(std::vector<ItemDefinition> items);
        void addPickup(Pickup pickup);
        std::vector<Pickup>& pickups();
        const std::vector<Pickup>& pickups() const;
        void collectPickup(std::size_t index);
        void setExit(LevelExit exit);
        const std::optional<LevelExit>& exit() const;
        std::optional<LevelExit>& exit();
        bool levelComplete() const;
        void completeLevel();

        double simulationTimeSeconds() const;
        std::optional<float> secondsSince(const std::optional<double>& timeSeconds) const;
        void advanceSimulationTime(float deltaTime);
        void emitNoise(NoiseEvent event);
        std::vector<NoiseEvent> takeNoises();

        ActorId addActor(Actor actor);
        bool removeActor(ActorId id);

        Actor* findActor(ActorId id);
        const Actor* findActor(ActorId id) const;

        std::vector<Actor>& actors();
        const std::vector<Actor>& actors() const;

        void addProjectile(Projectile projectile);
        bool removeProjectile(std::size_t index);

        std::vector<Projectile>& projectiles();
        const std::vector<Projectile>& projectiles() const;

        void addProjectileBurst(ProjectileBurst burst);
        bool removeProjectileBurst(std::size_t index);
        std::vector<ProjectileBurst>& projectileBursts();
        const std::vector<ProjectileBurst>& projectileBursts() const;

        void setPlayer(ActorId id, glm::vec2 spawnFeet);
        ActorId playerId() const;
        glm::vec2 playerSpawnFeet() const;
        void respawnPlayer();

        PlatformerConnectionCache& platformerConnections();
        const PlatformerConnectionCache& platformerConnections() const;

    private:
        void requireWithinSimulationTime(const std::optional<double>& time, const char* what) const;

        std::vector<ItemDefinition> itemDefinitions;
        PlatformerConnectionCache platformerConnectionCache;
        std::vector<Pickup> pickupStorage;
        std::optional<LevelExit> levelExit;
        bool completed = false;
        double elapsedSimulationTimeSeconds = 0.0;
        std::vector<Actor> actorStorage;
        std::vector<NoiseEvent> pendingNoises;
        std::vector<Projectile> projectileStorage;
        std::vector<ProjectileBurst> projectileBurstStorage;
        std::uint32_t nextActorId = 1;
        ActorId controlledPlayer;
        glm::vec2 controlledPlayerSpawnFeet = {0.0F, 0.0F};
    };
}
