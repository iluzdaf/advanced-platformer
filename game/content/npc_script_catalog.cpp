#include "npc_script_catalog.hpp"

#include "lua_npc_scripts.hpp"
#include "machine_catalog.hpp"

#include <filesystem>
#include <format>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_fact_rows.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"

namespace advanced_platformer
{
    namespace
    {
        void requireFileStem(const std::string& script)
        {
            const std::filesystem::path name(script);
            if (script == "." || script == ".." || name.has_root_path() || name.has_parent_path() ||
                name.filename() != name || name.has_extension())
            {
                throw std::invalid_argument(
                    std::format(
                        "Lua NPC script name '{}' must be a file stem, not a path", script));
            }
        }
    }

    namespace
    {
        void requireAnsweredConditions(
            const LuaNpcScripts& scripts,
            const std::string& machineName,
            const NpcStateMachine& machine)
        {
            std::map<std::string, std::string> scriptFacts;
            std::set<std::string> visited;
            for (const NpcMachineState& state : machine.states)
            {
                const std::string& script = state.does.script;
                if (!visited.insert(script).second)
                {
                    continue;
                }
                for (const std::string& fact : scripts.factNames(script))
                {
                    if (const auto [found, added] = scriptFacts.emplace(fact, script); !added)
                    {
                        throw std::invalid_argument(
                            std::format(
                                "Machine '{}' has the fact '{}' in both '{}' and '{}'",
                                machineName,
                                fact,
                                found->second,
                                script));
                    }
                }
            }
            for (const NpcMachineTransition& transition : machine.transitions)
            {
                for (const auto& [fact, asked] : transition.when)
                {
                    if (npcFactRow(fact) == nullptr && !scriptFacts.contains(fact))
                    {
                        throw std::invalid_argument(
                            std::format(
                                "The transition from '{}' to '{}' in machine '{}' asks about "
                                "'{}', which neither the engine nor its scripts answer",
                                transition.from,
                                transition.to,
                                machineName,
                                fact));
                    }
                }
            }
        }
    }

    void loadNpcActivityScripts(
        LuaNpcScripts& scripts,
        const MachineCatalog& machines,
        const std::filesystem::path& directory)
    {
        std::set<std::string> loaded;
        for (const auto& entry : machines)
        {
            const NpcStateMachine& machine = entry.second;
            for (const NpcMachineState& state : machine.states)
            {
                const NpcActivity& activity = state.does;
                if (!loaded.insert(activity.script).second)
                {
                    continue;
                }
                requireFileStem(activity.script);
                scripts.loadScript(activity.script, directory / (activity.script + ".lua"));
            }
        }

        for (const auto& [machineName, machine] : machines)
        {
            requireAnsweredConditions(scripts, machineName, machine);
            for (const NpcMachineState& state : machine.states)
            {
                const NpcActivity& activity = state.does;
                if (!scripts.hasActivity(activity))
                {
                    throw std::invalid_argument(
                        std::format(
                            "State '{}' in machine '{}' references unknown Lua activity '{}.{}'",
                            state.name,
                            machineName,
                            activity.script,
                            activity.activity));
                }
            }
        }
    }
}
