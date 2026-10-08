#include "machine_catalog.hpp"

#include "content_diagnostics.hpp"
#include "content_glaze.hpp"

#include <cstddef>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <glaze/glaze.hpp>

#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"

namespace advanced_platformer
{
    struct ActivityJson
    {
        std::string script;
        std::string activity;
    };

    struct MachineStateJson
    {
        std::string name;
        ActivityJson does;
    };

    struct MachineTransitionJson
    {
        std::variant<std::string, std::vector<std::string>> from;
        std::string to;
        std::map<std::string, bool> when;
        std::optional<float> after;
    };

    struct MachineJson
    {
        std::vector<MachineStateJson> states;
        std::vector<MachineTransitionJson> transitions;
        std::optional<bool> needsPatrol;
    };

    struct MachinesJson
    {
        std::map<std::string, MachineJson> machines;
    };

    namespace
    {
        std::string requireName(
            const std::string& name,
            std::string_view description,
            std::string_view sourceName,
            std::string_view path)
        {
            if (name.empty())
            {
                failJson(sourceName, path, std::format("{} cannot be empty", description));
            }
            return name;
        }

        NpcActivity npcActivity(
            const ActivityJson& json,
            std::string_view sourceName,
            const std::string& path)
        {
            return {
                requireName(json.script, "script name", sourceName, fieldPath(path, "script")),
                requireName(
                    json.activity, "activity name", sourceName, fieldPath(path, "activity"))};
        }

        NpcStateMachine machineFrom(
            const MachineJson& json,
            const std::string& name,
            std::string_view sourceName,
            const std::string& path)
        {
            NpcStateMachine machine;
            machine.name = name;
            machine.needsPatrol = json.needsPatrol.value_or(false);

            const std::string statesPath = fieldPath(path, "states");
            for (std::size_t index = 0; index < json.states.size(); ++index)
            {
                const MachineStateJson& stateJson = json.states[index];
                const std::string statePath = indexPath(statesPath, index);
                NpcMachineState state;
                state.name = requireName(
                    stateJson.name, "state name", sourceName, fieldPath(statePath, "name"));
                const std::string doesPath = fieldPath(statePath, "does");
                state.does = npcActivity(stateJson.does, sourceName, doesPath);
                machine.states.push_back(state);
            }

            const std::string transitionsPath = fieldPath(path, "transitions");
            for (std::size_t index = 0; index < json.transitions.size(); ++index)
            {
                const MachineTransitionJson& entry = json.transitions[index];
                const std::string transitionPath = indexPath(transitionsPath, index);
                NpcMachineTransition transition;
                transition.to = requireName(
                    entry.to, "state name", sourceName, fieldPath(transitionPath, "to"));
                transition.when = entry.when;
                if (entry.after.has_value())
                {
                    transition.after = *entry.after;
                }
                const std::string fromPath = fieldPath(transitionPath, "from");
                std::vector<std::string> sources;
                if (const auto* single = std::get_if<std::string>(&entry.from))
                {
                    sources.push_back(requireName(*single, "state name", sourceName, fromPath));
                }
                else
                {
                    const auto& list = std::get<std::vector<std::string>>(entry.from);
                    if (list.empty())
                    {
                        failJson(sourceName, fromPath, "expected at least one state name");
                    }
                    for (std::size_t source = 0; source < list.size(); ++source)
                    {
                        sources.push_back(requireName(
                            list[source], "state name", sourceName, indexPath(fromPath, source)));
                    }
                }
                for (const std::string& from : sources)
                {
                    transition.from = from;
                    machine.transitions.push_back(transition);
                }
            }
            return machine;
        }

        void validateMachineCatalog(const MachineCatalog& catalog)
        {
            for (const auto& entry : catalog)
            {
                try
                {
                    if (entry.first.empty())
                    {
                        throw std::invalid_argument("machine name cannot be empty");
                    }
                    validateNpcStateMachine(entry.second);
                }
                catch (const std::invalid_argument& error)
                {
                    failJson({}, fieldPath("machines", entry.first), error.what());
                }
            }
        }
    }

    MachineCatalog parseMachineCatalog(std::string_view text, std::string_view sourceName)
    {
        const auto file = readContent<MachinesJson>(text, sourceName);
        MachineCatalog catalog;
        for (const auto& [name, json] : file.machines)
        {
            catalog.emplace(name, machineFrom(json, name, sourceName, fieldPath("machines", name)));
        }
        validateInFile(sourceName, [&] { validateMachineCatalog(catalog); });
        return catalog;
    }

    MachineCatalog loadMachineCatalog(const std::filesystem::path& path)
    {
        return parseMachineCatalog(loadContentText(path), path.string());
    }

    const NpcStateMachine& npcStateMachine(const MachineCatalog& catalog, const std::string& name)
    {
        const auto found = catalog.find(name);
        if (found == catalog.end())
        {
            throw std::invalid_argument(std::format("unknown state machine '{}'", name));
        }
        return found->second;
    }
}
