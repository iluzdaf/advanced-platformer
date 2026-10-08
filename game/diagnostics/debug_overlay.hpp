#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/navigation/traversal.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/render/sprite.hpp"

#include "diagnostics/navigation_debug.hpp"

namespace advanced_platformer
{
    class World;
    class TileMap;
    struct CameraController;
    enum class AnimationName;

    enum class ActorDebugKind
    {
        Player,
        Npc,
        Actor
    };

    struct ActorSpriteDebugInfo
    {
        Aabb bounds;
        SpriteRegion atlasRegion;
    };

    struct PathConnectionDebugInfo
    {
        glm::vec2 fromFeet = {0.0F, 0.0F};
        glm::vec2 toFeet = {0.0F, 0.0F};
        Traversal traversal = Traversal::Fly;
        bool completed = false;
        bool next = false;
        std::vector<glm::vec2> sampledFeet;
    };

    struct PathFollowerDebugInfo
    {
        bool hasPath = false;
        std::size_t nextStep = 0;
        std::size_t stepCount = 0;
        std::optional<glm::vec2> goalFeet;
        std::vector<PathConnectionDebugInfo> connections;
    };

    struct SensorDebugInfo
    {
        glm::vec2 observerCenter = {0.0F, 0.0F};
        float noticeDistance = 0.0F;
        std::optional<glm::vec2> visibleTargetCenter;
        std::optional<glm::vec2> rememberedTargetFeet;
        float memoryRemaining = 0.0F;
    };

    struct PatrolDebugInfo
    {
        glm::vec2 firstFeet = {0.0F, 0.0F};
        glm::vec2 secondFeet = {0.0F, 0.0F};
        bool headingToSecond = true;
    };

    struct ActorDebugInfo
    {
        ActorId id;
        ActorDebugKind kind = ActorDebugKind::Actor;
        std::optional<std::string> definitionName;
        Aabb collider;
        std::optional<ActorSpriteDebugInfo> sprite;
        std::optional<AnimationName> animation;
        std::optional<std::string> machineState;
        std::optional<PathFollowerDebugInfo> pathFollower;
        std::optional<SensorDebugInfo> sensor;
        std::optional<PatrolDebugInfo> patrol;
        std::optional<Aabb> biteHitbox;
    };

    struct ProjectileDebugInfo
    {
        Aabb bounds;
        float lifetimeRemaining = 0.0F;
        std::optional<ActorId> owner;
    };

    struct PickupDebugInfo
    {
        Aabb bounds;
        std::string itemName;
    };

    struct MachineDebugInfo
    {
        ActorId actor;
        NpcStateMachine definition;
        std::size_t active = 0;
        std::optional<std::size_t> lastFired;
    };

    struct DebugOverlay
    {
        std::vector<ActorDebugInfo> actors;
        std::vector<ProjectileDebugInfo> projectiles;
        std::vector<PickupDebugInfo> pickups;
        std::optional<NavigationCacheDebugInfo> navigationCache;
        std::optional<Aabb> breakableCellUnderCursor;
        std::optional<MachineDebugInfo> machine;
        Aabb cameraBounds;
        Aabb cameraDeadZone;
    };

    DebugOverlay makeDebugOverlay(
        const World& world,
        const TileMap& map,
        const CameraController& cameraController,
        float simulationStepSeconds,
        const NavigationDebugView& navigation = {},
        std::optional<ActorId> lockedMachineActor = std::nullopt);
}
