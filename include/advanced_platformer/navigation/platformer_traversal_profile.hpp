#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"

namespace advanced_platformer
{
    // Static movement capabilities and the fixed simulation step. This is not live
    // actor state; matching profiles share cached connections.
    struct PlatformerTraversalProfile
    {
        glm::vec2 size = {0.0F, 0.0F};
        PlatformerMovementConfig movement;
        float stepSeconds = 0.0F;
        std::optional<SurfaceClimbConfig> climb;

        bool operator==(const PlatformerTraversalProfile&) const = default;
    };
}
