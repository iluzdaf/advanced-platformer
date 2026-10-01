#pragma once

#include <string>
#include <utility>

#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"

namespace tests
{
    // An activity for a test machine; tests answer it with RecordingNpcScripts or an inline
    // fixture script.
    inline advanced_platformer::NpcActivity testActivity(std::string activity)
    {
        return {"test", std::move(activity)};
    }

    // Builds a data-driven machine in the words the json uses, in the order the machine
    // lists them:
    //
    //   NpcMachineBuilder::named("test")
    //       .state("rest", testActivity("idle"))
    //       .state("hunt", testActivity("chase"))
    //       .transition("rest", "hunt").when("targetKnown", true)
    //       .transition("hunt", "rest").when("targetKnown", false).after(0.5F)
    //
    // The order can't be broken: named() offers only state(), a state offers more states
    // or the first transition, and when() and after() exist only on a transition, which
    // they describe. States come before transitions, as in the json. The builder holds no
    // rules beyond that: startNpcMachine validates what it built. A machine with at least
    // one state converts to an NpcStateMachine wherever one is expected, such as the
    // actor builder's running().
    //
    // Each stage of the builder holds the machine built so far. The stages share it through
    // this base rather than deriving from one another, so no stage inherits another's steps.
    class NpcMachineDraft
    {
    protected:
        advanced_platformer::NpcStateMachine built;
    };

    class NpcMachineBuilder : public NpcMachineDraft
    {
    public:
        class Stated;
        class Transitioning;

        static NpcMachineBuilder named(std::string name)
        {
            NpcMachineBuilder builder;
            builder.built.name = std::move(name);
            return builder;
        }

        Stated state(std::string name, advanced_platformer::NpcActivity does) &&;

    private:
        NpcMachineBuilder() = default;
    };

    // A machine with its states so far, waiting for more or for its first transition.
    class NpcMachineBuilder::Stated : public NpcMachineDraft
    {
    public:
        Stated state(std::string name, advanced_platformer::NpcActivity does) &&
        {
            built.states.push_back({std::move(name), std::move(does)});
            return std::move(*this);
        }

        Transitioning transition(std::string from, std::string to) &&;

        operator advanced_platformer::NpcStateMachine() &&
        {
            return std::move(built);
        }

    private:
        friend class NpcMachineBuilder;

        explicit Stated(advanced_platformer::NpcStateMachine machine)
        {
            built = std::move(machine);
        }
    };

    // A machine whose last transition is the one when() and after() describe.
    class NpcMachineBuilder::Transitioning : public NpcMachineDraft
    {
    public:
        Transitioning when(const std::string& fact, bool holds) &&
        {
            built.transitions.back().when[fact] = holds;
            return std::move(*this);
        }

        Transitioning after(float seconds) &&
        {
            built.transitions.back().after = seconds;
            return std::move(*this);
        }

        Transitioning transition(std::string from, std::string to) &&
        {
            advanced_platformer::NpcMachineTransition transition;
            transition.from = std::move(from);
            transition.to = std::move(to);
            built.transitions.push_back(std::move(transition));
            return std::move(*this);
        }

        operator advanced_platformer::NpcStateMachine() &&
        {
            return std::move(built);
        }

    private:
        friend class Stated;

        explicit Transitioning(advanced_platformer::NpcStateMachine machine)
        {
            built = std::move(machine);
        }
    };

    inline NpcMachineBuilder::Stated NpcMachineBuilder::state(
        std::string name,
        advanced_platformer::NpcActivity does) &&
    {
        built.states.push_back({std::move(name), std::move(does)});
        return Stated(std::move(built));
    }

    inline NpcMachineBuilder::Transitioning NpcMachineBuilder::Stated::transition(
        std::string from,
        std::string to) &&
    {
        advanced_platformer::NpcMachineTransition transition;
        transition.from = std::move(from);
        transition.to = std::move(to);
        built.transitions.push_back(std::move(transition));
        return Transitioning(std::move(built));
    }
}
