#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

#include "advanced_platformer/npc/npc_state_machine.hpp"

namespace advanced_platformer
{
    using MachineCatalog = std::map<std::string, NpcStateMachine>;

    MachineCatalog parseMachineCatalog(std::string_view text, std::string_view sourceName);
    MachineCatalog loadMachineCatalog(const std::filesystem::path& path);
    const NpcStateMachine& npcStateMachine(const MachineCatalog& catalog, const std::string& name);
}
