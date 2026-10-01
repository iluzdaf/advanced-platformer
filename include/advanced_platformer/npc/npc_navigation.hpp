#pragma once

#include <glm/vec2.hpp>

#include "advanced_platformer/input/input_state.hpp"

namespace advanced_platformer
{
    struct Actor;
    struct NpcUpdate;
    struct PathFollower;

    // This tick's intentions towards the goal: plans a path when there is none, when the
    // goal has moved, when the actor has been moved off a finished path, or when a tile
    // has broken since, then follows it. Only movement is set, so a caller that also aims
    // does so afterwards.
    InputIntentions intentionsToReach(
        const NpcUpdate& update,
        Actor& actor,
        PathFollower& follower,
        glm::vec2 goalFeet);
}
