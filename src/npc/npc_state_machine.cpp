#include "advanced_platformer/npc/npc_state_machine.hpp"

#include <cstddef>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_fact_rows.hpp"
#include "advanced_platformer/npc/npc_facts.hpp"

namespace advanced_platformer
{
    namespace
    {
        std::string describe(const NpcMachineTransition& transition)
        {
            return std::format(
                "the transition from \"{}\" to \"{}\"", transition.from, transition.to);
        }

        bool hasState(const NpcStateMachine& machine, std::string_view name)
        {
            for (const NpcMachineState& state : machine.states)
            {
                if (state.name == name)
                {
                    return true;
                }
            }
            return false;
        }

        // Whether every condition holds for these facts. A fact no row answers is an error.
        bool conditionsHold(const std::map<std::string, bool>& when, const NpcFacts& facts)
        {
            for (const auto& [fact, asked] : when)
            {
                const NpcFactRow* row = npcFactRow(fact);
                if (row == nullptr)
                {
                    throw std::logic_error(
                        std::format(
                            "A condition asks about \"{}\", and there is no such fact", fact));
                }
                if (row->holds(facts) != asked)
                {
                    return false;
                }
            }
            return true;
        }
    }

    void validateNpcStateMachine(const NpcStateMachine& machine)
    {
        if (machine.states.empty())
        {
            throw std::invalid_argument("A state machine needs at least one state");
        }
        std::set<std::string> names;
        for (const NpcMachineState& state : machine.states)
        {
            if (state.name.empty())
            {
                throw std::invalid_argument("A state machine state needs a name");
            }
            if (!names.insert(state.name).second)
            {
                throw std::invalid_argument(
                    std::format("The state \"{}\" is declared twice", state.name));
            }
            if (const auto* scripted = std::get_if<LuaNpcActivity>(&state.does))
            {
                if (scripted->script.empty())
                {
                    throw std::invalid_argument(
                        std::format("The state \"{}\" needs a Lua script name", state.name));
                }
                if (scripted->activity.empty())
                {
                    throw std::invalid_argument(
                        std::format("The state \"{}\" needs a Lua activity name", state.name));
                }
            }
        }
        for (const NpcMachineTransition& transition : machine.transitions)
        {
            if (!hasState(machine, transition.from))
            {
                throw std::invalid_argument(
                    std::format("{} starts from a state the machine lacks", describe(transition)));
            }
            if (!hasState(machine, transition.to))
            {
                throw std::invalid_argument(
                    std::format("{} leads to a state the machine lacks", describe(transition)));
            }
            for (const auto& [fact, asked] : transition.when)
            {
                if (npcFactRow(fact) == nullptr)
                {
                    throw std::invalid_argument(
                        std::format(
                            "{} asks about \"{}\", and there is no such fact",
                            describe(transition),
                            fact));
                }
            }
            if (!isFiniteNonNegative(transition.after))
            {
                throw std::invalid_argument(
                    std::format(
                        "{} must hold for a finite, non-negative time", describe(transition)));
            }
        }
    }

    std::size_t npcMachineStateNamed(const NpcStateMachine& machine, std::string_view name)
    {
        for (std::size_t index = 0; index < machine.states.size(); ++index)
        {
            if (machine.states[index].name == name)
            {
                return index;
            }
        }
        throw std::invalid_argument(std::format("The machine has no state \"{}\"", name));
    }

    NpcMachine startNpcMachine(NpcStateMachine definition)
    {
        validateNpcStateMachine(definition);
        NpcMachine machine;
        machine.heldFor.assign(definition.transitions.size(), 0.0F);
        machine.definition = std::move(definition);
        return machine;
    }

    std::optional<std::size_t> advanceNpcMachine(
        NpcMachine& machine,
        const NpcFacts& facts,
        float deltaTime)
    {
        requireSeconds(deltaTime, "State machine time step");
        const NpcStateMachine& definition = machine.definition;
        if (machine.active >= definition.states.size() ||
            machine.heldFor.size() != definition.transitions.size())
        {
            throw std::logic_error("A state machine was not started");
        }
        const std::string& from = definition.states[machine.active].name;
        for (std::size_t index = 0; index < definition.transitions.size(); ++index)
        {
            const NpcMachineTransition& transition = definition.transitions[index];
            if (transition.from != from)
            {
                continue;
            }
            if (!conditionsHold(transition.when, facts))
            {
                machine.heldFor[index] = 0.0F;
                continue;
            }
            machine.heldFor[index] += deltaTime;
            if (machine.heldFor[index] < transition.after)
            {
                continue;
            }
            machine.active = npcMachineStateNamed(definition, transition.to);
            machine.heldFor.assign(definition.transitions.size(), 0.0F);
            machine.stateElapsed = 0.0F;
            machine.lastFired = index;
            return index;
        }
        return std::nullopt;
    }

    const NpcMachineState& activeNpcMachineState(const NpcMachine& machine)
    {
        if (machine.active >= machine.definition.states.size())
        {
            throw std::logic_error("A state machine was not started");
        }
        return machine.definition.states[machine.active];
    }
}
