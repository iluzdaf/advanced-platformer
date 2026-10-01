#pragma once

#include <string>

namespace advanced_platformer
{
    // What an NPC does while a machine state is active: an activity in a Lua script.
    struct LuaNpcActivity
    {
        std::string script;
        std::string activity;

        bool operator==(const LuaNpcActivity&) const = default;
    };
}
