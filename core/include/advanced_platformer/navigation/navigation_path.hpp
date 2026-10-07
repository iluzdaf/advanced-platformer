#pragma once

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/input/input_program.hpp"
#include "advanced_platformer/navigation/traversal.hpp"

namespace advanced_platformer
{
    struct Waypoint
    {
        glm::vec2 feet{0.0F, 0.0F};
        Traversal traversal = Traversal::Fly;
        InputProgram inputs;
    };

    struct NavigationPath
    {
        glm::vec2 startFeet{0.0F, 0.0F};
        std::vector<Waypoint> waypoints;
    };

    inline glm::vec2 endOf(const NavigationPath& path)
    {
        return path.waypoints.empty() ? path.startFeet : path.waypoints.back().feet;
    }

    enum class NavigationPathStatus
    {
        Found,
        Unreachable,
        Deferred
    };

    struct NavigationPathResult
    {
        NavigationPathStatus status = NavigationPathStatus::Unreachable;
        std::optional<NavigationPath> path;
    };
}
