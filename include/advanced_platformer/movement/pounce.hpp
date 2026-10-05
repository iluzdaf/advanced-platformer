#pragma once

#include <optional>
#include <vector>

#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/physics/collision.hpp"

namespace advanced_platformer
{
    class TileMap;
    struct Body;
    struct InputIntentions;
    struct PlatformerMovement;

    enum class PouncePhase
    {
        Ready,
        Airborne,
        Recovery
    };

    struct PounceConfig
    {
        float speed = 200.0F;
        float lift = 120.0F;
        float range = 64.0F;
        float recoveryDuration = 0.5F;
        int damage = 1;
        std::optional<Knockback> knockback;

        bool operator==(const PounceConfig&) const = default;
    };

    struct Pounce
    {
        PounceConfig config;
        PouncePhase phase = PouncePhase::Ready;
        float phaseTimeRemaining = 0.0F;
        ClimbSurface launchedFrom = ClimbSurface::None;
        std::vector<ActorId> actorsHit;
    };

    void validatePounceConfig(const PounceConfig& config);

    CollisionContacts updatePounceMovement(
        const TileMap& map,
        Body& body,
        PlatformerMovement& movement,
        SurfaceClimb* climb,
        Pounce& pounce,
        const InputIntentions& intentions,
        bool pressed,
        float deltaTime);
}
