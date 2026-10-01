#include "npc_script_catalog.hpp"

#include "machine_catalog.hpp"

#include <filesystem>
#include <format>
#include <set>
#include <stdexcept>
#include <string>
#include <variant>

#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "lua_npc_scripts.hpp"

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
                const auto* activity = std::get_if<LuaNpcActivity>(&state.does);
                if (activity == nullptr || !loaded.insert(activity->script).second)
                {
                    continue;
                }
                requireFileStem(activity->script);
                scripts.loadScript(activity->script, directory / (activity->script + ".lua"));
            }
        }

        for (const auto& [machineName, machine] : machines)
        {
            for (const NpcMachineState& state : machine.states)
            {
                const auto* activity = std::get_if<LuaNpcActivity>(&state.does);
                if (activity != nullptr && !scripts.hasActivity(*activity))
                {
                    throw std::invalid_argument(
                        std::format(
                            "State '{}' in machine '{}' references unknown Lua activity '{}.{}'",
                            state.name,
                            machineName,
                            activity->script,
                            activity->activity));
                }
            }
        }
    }
}
