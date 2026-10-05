#pragma once

namespace advanced_platformer
{
    class TileMap;
    struct Actor;
    struct NpcBrain;
    struct NpcPerception;

    struct NpcFacts
    {
        bool targetKnown = false;
        bool targetVisible = false;
        bool targetInBiteRange = false;
        bool biteReady = false;
        bool targetInSights = false;
        bool targetWithinStandoffDistance = false;
        bool heardLanding = false;
        bool targetOnSameSurface = false;
        bool targetWithinNoticeDistance = false;
        bool movementBlocked = false;
        bool hasPatrol = false;
        bool searches = false;
        bool searchTimeUp = false;
        float stateElapsed = 0.0F;
    };

    NpcFacts gatherNpcFacts(
        const TileMap& map,
        const Actor& actor,
        const NpcBrain& brain,
        const NpcPerception& perception,
        const Actor* target,
        float stateElapsed);
}
