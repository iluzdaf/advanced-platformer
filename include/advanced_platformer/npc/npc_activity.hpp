#pragma once

#include <string>

namespace advanced_platformer
{
    // What an NPC does while a machine state is active: an activity in a Lua script.
    struct NpcActivity
    {
        std::string script;
        std::string activity;

        bool operator==(const NpcActivity&) const = default;
    };
}
