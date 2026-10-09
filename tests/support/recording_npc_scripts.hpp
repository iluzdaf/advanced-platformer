#pragma once

#include <map>
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

        bool hasFacts(const std::string& script) const override
        {
            return factAnswers.contains(script);
        }

        std::map<std::string, bool> facts(
            advanced_platformer::ActorId actor,
            const std::string& script,
            const advanced_platformer::NpcActivitySnapshot& snapshot,
            float) override
        {
            calls.push_back({"fact", actor, {script, {}}, snapshot});
            return factAnswers.at(script);
        }

        void forget(advanced_platformer::ActorId actor) override
        {
            forgotten.push_back(actor);
        }

        advanced_platformer::NpcActivityCommand command;
        std::map<std::string, std::map<std::string, bool>> factAnswers;
        std::vector<ScriptCall> calls;
        std::vector<float> updateSteps;
        std::vector<advanced_platformer::ActorId> forgotten;
    };
}
