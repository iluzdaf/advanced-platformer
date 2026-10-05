#pragma once

#include "advanced_platformer/npc/npc_facts.hpp"

namespace tests
{
    class NpcFactsBuilder
    {
    public:
        static NpcFactsBuilder facts()
        {
            return {};
        }

        NpcFactsBuilder withPatrol() &&
        {
            built.hasPatrol = true;
            return *this;
        }

        NpcFactsBuilder knowingTarget() &&
        {
            built.targetKnown = true;
            return *this;
        }

        NpcFactsBuilder targetInPrimaryRange() &&
        {
            built.targetKnown = true;
            built.targetVisible = true;
            built.targetInPrimaryRange = true;
            return *this;
        }

        NpcFactsBuilder targetWithinStandoffDistance() &&
        {
            built.targetKnown = true;
            built.targetWithinStandoffDistance = true;
            return *this;
        }

        NpcFactsBuilder searching() &&
        {
            built.searches = true;
            return *this;
        }

        NpcFactsBuilder searchTimeUp() &&
        {
            built.searches = true;
            built.searchTimeUp = true;
            return *this;
        }

        NpcFactsBuilder primaryReadyFor(float stateElapsed) &&
        {
            built.primaryReady = true;
            built.stateElapsed = stateElapsed;
            return *this;
        }

        operator advanced_platformer::NpcFacts() &&
        {
            return built;
        }

    private:
        advanced_platformer::NpcFacts built;
    };
}
