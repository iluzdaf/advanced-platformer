#pragma once

#include <string>
#include <variant>

#include "advanced_platformer/npc/npc.hpp"

namespace advanced_platformer
{
    struct BuiltInNpcActivity
    {
        NpcState state = NpcState::Idle;

        bool operator==(const BuiltInNpcActivity&) const = default;
    };

    struct LuaNpcActivity
    {
        std::string script;
        std::string activity;

        bool operator==(const LuaNpcActivity&) const = default;
    };

    using NpcActivity = std::variant<BuiltInNpcActivity, LuaNpcActivity>;
}
