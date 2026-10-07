#include "advanced_platformer/npc/npc_fact_rows.hpp"

#include <string_view>
#include <vector>

#include "advanced_platformer/npc/npc_facts.hpp"

namespace advanced_platformer
{
    const std::vector<NpcFactRow>& npcFactRows()
    {
        static const std::vector<NpcFactRow> rows{
            {"targetKnown",
             "target known",
             "no target",
             [](const NpcFacts& facts) { return facts.targetKnown; }},
            {"targetVisible",
             "target in sight",
             "target out of sight",
             [](const NpcFacts& facts) { return facts.targetVisible; }},
            {"targetInPrimaryRange",
             "target in primary range",
             "target out of primary range",
             [](const NpcFacts& facts) { return facts.targetInPrimaryRange; }},
            {"primaryReady",
             "primary attack ready",
             "primary attack not ready",
             [](const NpcFacts& facts) { return facts.primaryReady; }},
            {"primaryActive",
             "primary attack active",
             "primary attack not active",
             [](const NpcFacts& facts) { return facts.primaryActive; }},
            {"targetInSecondaryRange",
             "target in secondary range",
             "target out of secondary range",
             [](const NpcFacts& facts) { return facts.targetInSecondaryRange; }},
            {"secondaryReady",
             "secondary attack ready",
             "secondary attack not ready",
             [](const NpcFacts& facts) { return facts.secondaryReady; }},
            {"secondaryActive",
             "secondary attack active",
             "secondary attack not active",
             [](const NpcFacts& facts) { return facts.secondaryActive; }},
            {"targetWithinStandoffDistance",
             "target within standoff distance",
             "target at or beyond standoff distance or unknown",
             [](const NpcFacts& facts) { return facts.targetWithinStandoffDistance; }},
            {"heardLanding",
             "heard a landing",
             "no landing heard",
             [](const NpcFacts& facts) { return facts.heardLanding; }},
            {"targetOnSameSurface",
             "target on this surface",
             "target off this surface",
             [](const NpcFacts& facts) { return facts.targetOnSameSurface; }},
            {"targetWithinNoticeDistance",
             "target within notice distance",
             "target beyond notice distance or unknown",
             [](const NpcFacts& facts) { return facts.targetWithinNoticeDistance; }},
            {"movementBlocked",
             "movement blocked",
             "movement not blocked",
             [](const NpcFacts& facts) { return facts.movementBlocked; }},
            {"hasPatrol",
             "has a patrol",
             "no patrol",
             [](const NpcFacts& facts) { return facts.hasPatrol; }},
            {"searches",
             "searches",
             "does not search",
             [](const NpcFacts& facts) { return facts.searches; }},
            {"searchTimeUp",
             "search time up",
             "still searching",
             [](const NpcFacts& facts) { return facts.searchTimeUp; }},
        };
        return rows;
    }

    const NpcFactRow* npcFactRow(std::string_view name)
    {
        for (const NpcFactRow& row : npcFactRows())
        {
            if (row.name == name)
            {
                return &row;
            }
        }
        return nullptr;
    }
}
