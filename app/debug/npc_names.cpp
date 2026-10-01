#include "npc_names.hpp"

#include <format>
#include <string>

#include "advanced_platformer/npc/npc_activity.hpp"

namespace advanced_platformer
{
    std::string nameOf(const LuaNpcActivity& activity)
    {
        return std::format("{}.{}", activity.script, activity.activity);
    }
}
