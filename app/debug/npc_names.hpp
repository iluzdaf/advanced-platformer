#pragma once

#include <string>

#include "advanced_platformer/npc/npc_activity.hpp"

namespace advanced_platformer
{
    // The name the machine window prints for a state's activity: "script.activity".
    std::string nameOf(const NpcActivity& activity);
}
