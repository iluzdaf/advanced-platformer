#pragma once

#include <optional>
#include <string>

#include "advanced_platformer/actor/actor_id.hpp"

namespace advanced_platformer
{
    enum class LuaScriptDiagnosticKind
    {
        Error,
        Print,
    };

    struct LuaScriptDiagnostic
    {
        std::string source;
        std::string script;
        std::string activity;
        std::string hook;
        std::optional<ActorId> actor;
        std::string message;
        LuaScriptDiagnosticKind kind = LuaScriptDiagnosticKind::Error;
    };
}
