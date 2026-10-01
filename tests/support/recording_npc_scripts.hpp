#pragma once

#include <string>
#include <vector>

#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_activity_scripts.hpp"

namespace tests
{
    struct ScriptCall
    {
        std::string hook;
        advanced_platformer::ActorId actor;
        advanced_platformer::NpcActivity activity;
        advanced_platformer::NpcActivitySnapshot snapshot;
    };

    // Stands in for the Lua runtime: records every hook it is called with, and answers
    // each update with the same command.
    class RecordingNpcScripts final : public advanced_platformer::NpcActivityScripts
    {
    public:
        void enter(
            advanced_platformer::ActorId actor,
            const advanced_platformer::NpcActivity& activity,
            const advanced_platformer::NpcActivitySnapshot& snapshot) override
        {
            calls.push_back({"enter", actor, activity, snapshot});
        }

        advanced_platformer::NpcActivityCommand update(
            advanced_platformer::ActorId actor,
            const advanced_platformer::NpcActivity& activity,
            const advanced_platformer::NpcActivitySnapshot& snapshot,
            float deltaTime) override
        {
            updateSteps.push_back(deltaTime);
            calls.push_back({"update", actor, activity, snapshot});
            return command;
        }

        void exit(
            advanced_platformer::ActorId actor,
            const advanced_platformer::NpcActivity& activity,
            const advanced_platformer::NpcActivitySnapshot& snapshot) override
        {
            calls.push_back({"exit", actor, activity, snapshot});
        }

        void forget(advanced_platformer::ActorId actor) override
        {
            forgotten.push_back(actor);
        }

        advanced_platformer::NpcActivityCommand command;
        std::vector<ScriptCall> calls;
        std::vector<float> updateSteps;
        std::vector<advanced_platformer::ActorId> forgotten;
    };
}
