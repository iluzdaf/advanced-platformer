#pragma once

#include <cstddef>
#include <optional>

#include <glm/vec2.hpp>

#include "advanced_platformer/navigation/navigation_path.hpp"

namespace advanced_platformer
{
    struct Aabb;
    struct Body;
    struct FlyingMovement;
    struct InputIntentions;
    struct PlatformerMovement;
    struct SurfaceClimb;

    struct PathFollower
    {
        std::optional<NavigationPath> path;
        std::size_t nextStep = 0;
        float programElapsed = 0.0F;
        std::optional<glm::vec2> goal;
        std::size_t breaksWhenPlanned = 0;
        std::optional<NavigationPathStatus> routeStatus;
    };

    void setPath(PathFollower& follower, NavigationPath path);
    void clearPath(PathFollower& follower);
    bool pathComplete(const PathFollower& follower);

    InputIntentions followFlyingPath(
        const Aabb& bounds,
        const FlyingMovement& movement,
        PathFollower& follower,
        float deltaTime);

    InputIntentions followPlatformerPath(
        const Body& body,
        const PlatformerMovement& movement,
        PathFollower& follower,
        float deltaTime,
        const SurfaceClimb* climb = nullptr);
}
