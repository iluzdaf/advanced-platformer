#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor_id.hpp"

namespace advanced_platformer
{
    struct NpcBrain
    {
        std::optional<ActorId> target;
        glm::vec2 lastKnownTargetFeet = {0.0F, 0.0F};
        float targetMemoryRemaining = 0.0F;
    };

    struct NpcPerception
    {
        bool targetVisible = false;
        bool heardLanding = false;
    };

    struct NpcSenses
    {
        float noticeDistance = 96.0F;
        float targetMemoryDuration = 1.5F;
        float searchDuration = 2.0F;
        float standoffDistance = 48.0F;
    };

    struct Patrol
    {
        glm::vec2 firstFeet = {0.0F, 0.0F};
        glm::vec2 secondFeet = {0.0F, 0.0F};
        bool headingToSecond = true;
    };
}
