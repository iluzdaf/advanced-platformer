#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_facts.hpp"

namespace advanced_platformer
{
    struct NpcMachineState
    {
        std::string name;
        NpcActivity does;
    };

    struct NpcMachineTransition
    {
        std::string from;
        std::string to;
        std::map<std::string, bool> when;
        float after = 0.0F;
    };

    struct NpcStateMachine
    {
        std::string name;
        std::vector<NpcMachineState> states;
        std::vector<NpcMachineTransition> transitions;
    };

    void validateNpcStateMachine(const NpcStateMachine& machine);
    std::size_t npcMachineStateNamed(const NpcStateMachine& machine, std::string_view name);

    struct NpcMachine
    {
        NpcStateMachine definition;
        std::size_t active = 0;
        std::vector<float> heldFor;
        float stateElapsed = 0.0F;
        bool activityEntered = false;
        std::optional<std::size_t> lastFired;
    };

    NpcMachine startNpcMachine(NpcStateMachine definition);
    std::optional<std::size_t> advanceNpcMachine(
        NpcMachine& machine,
        const NpcFacts& facts,
        float deltaTime);
    const NpcMachineState& activeNpcMachineState(const NpcMachine& machine);
}
