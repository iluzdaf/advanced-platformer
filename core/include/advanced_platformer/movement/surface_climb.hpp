#pragma once

#include "advanced_platformer/physics/collision.hpp"

namespace advanced_platformer
{
    class TileMap;
    struct Aabb;
    struct Body;
    struct InputIntentions;
    struct PlatformerMovement;

    enum class ClimbSurface
    {
        None,
        LeftWall,
        RightWall,
        Ceiling
    };

    enum class WallHeading
    {
        Up,
        Down
    };

    struct SurfaceClimbConfig
    {
        float speed = 60.0F;

        bool operator==(const SurfaceClimbConfig&) const = default;
    };

    struct SurfaceClimb
    {
        SurfaceClimbConfig config;
        ClimbSurface surface = ClimbSurface::None;
        WallHeading wallHeading = WallHeading::Up;
    };

    void validateSurfaceClimbConfig(const SurfaceClimbConfig& config);

    WallHeading wallHeadingFor(
        ClimbSurface surface,
        const InputIntentions& intentions,
        WallHeading current);

    bool touchesClimbable(const TileMap& map, const Aabb& bounds, ClimbSurface surface);

    void gripNearbySurface(const TileMap& map, Aabb& bounds, SurfaceClimb& climb);

    CollisionContacts updateSurfaceClimbMovement(
        const TileMap& map,
        Body& body,
        PlatformerMovement& movement,
        SurfaceClimb& climb,
        const InputIntentions& intentions,
        float deltaTime);
}
