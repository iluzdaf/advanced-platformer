#pragma once

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

        bool operator==(const PounceConfig&) const = default;
    };

    struct Pounce
    {
        PounceConfig config;
        PouncePhase phase = PouncePhase::Ready;
        float phaseTimeRemaining = 0.0F;
        ClimbSurface launchedFrom = ClimbSurface::None;
    };

    void validatePounceConfig(const PounceConfig& config);

    CollisionContacts updatePounceMovement(
        const TileMap& map,
        Body& body,
        PlatformerMovement& movement,
        SurfaceClimb* climb,
        Pounce& pounce,
        const InputIntentions& intentions,
        float deltaTime);
}
