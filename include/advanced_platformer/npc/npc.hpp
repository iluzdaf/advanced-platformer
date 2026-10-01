#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor_id.hpp"

namespace advanced_platformer
{
    // What an NPC knows of its target, retained between sensing updates. Its state machine
    // holds what it is doing.
    struct NpcBrain
    {
        std::optional<ActorId> target;
        // Last observed target feet, refreshed by sight or an eligible noise.
        glm::vec2 lastKnownTargetFeet = {0.0F, 0.0F};
        float targetMemoryRemaining = 0.0F;
    };

    // Transient observations, replaced on every sensing update. Behaviour copies
    // these into NpcFacts alongside facts derived from other actor components.
    struct NpcPerception
    {
        bool targetVisible = false;
        // A player landing heard on this run during the latest sensing update.
        bool heardLanding = false;
    };

    // Its member names are the keys of an actor's "senses" object in actors.json, so
    // renaming one renames the key.
    struct NpcSenses
    {
        float noticeDistance = 96.0F;
        float targetMemoryDuration = 1.5F;
        // How long a lost target is searched for before the NPC returns to its routine.
        // Zero sends it straight back.
        float searchDuration = 2.0F;
        // Threshold for targetWithinStandoffDistance; machines decide what to do when the
        // target crosses it.
        float standoffDistance = 48.0F;
    };

    struct Patrol
    {
        glm::vec2 firstFeet = {0.0F, 0.0F};
        glm::vec2 secondFeet = {0.0F, 0.0F};
        bool headingToSecond = true;
    };
}
