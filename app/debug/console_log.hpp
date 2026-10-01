#pragma once

#include <cstddef>
#include <deque>
#include <ostream>
#include <string>

#include "lua_npc_scripts.hpp"

namespace advanced_platformer
{
    enum class ConsoleLevel
    {
        Info,
        Error,
    };

    struct ConsoleEntry
    {
        ConsoleLevel level = ConsoleLevel::Info;
        std::string text;
    };

    class ConsoleLog
    {
    public:
        explicit ConsoleLog(std::ostream& echo, std::size_t capacity = 500);

        void write(ConsoleLevel level, std::string text);
        const std::deque<ConsoleEntry>& entries() const;
        std::size_t written() const;

    private:
        std::ostream* echo;
        std::size_t capacity;
        std::deque<ConsoleEntry> kept;
        std::size_t total = 0;
    };

    void writeScriptDiagnostic(ConsoleLog& log, const LuaScriptDiagnostic& diagnostic);
}
