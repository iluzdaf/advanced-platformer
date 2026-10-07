#include "console_log.hpp"

#include <cstddef>
#include <deque>
#include <format>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "lua_script_diagnostic.hpp"

namespace advanced_platformer
{
    ConsoleLog::ConsoleLog(std::ostream& echo, std::size_t capacity)
        : echo(&echo),
          capacity(capacity)
    {
        if (capacity == 0)
        {
            throw std::invalid_argument("A console log must keep at least one entry");
        }
    }

    void ConsoleLog::write(ConsoleLevel level, std::string text)
    {
        *echo << text << '\n';
        echo->flush();
        if (kept.size() == capacity)
        {
            kept.pop_front();
        }
        kept.push_back({level, std::move(text)});
        ++total;
    }

    const std::deque<ConsoleEntry>& ConsoleLog::entries() const
    {
        return kept;
    }

    std::size_t ConsoleLog::written() const
    {
        return total;
    }

    void writeScriptDiagnostic(ConsoleLog& log, const LuaScriptDiagnostic& diagnostic)
    {
        std::string where = diagnostic.script;
        if (!diagnostic.activity.empty())
        {
            where += std::format(".{}", diagnostic.activity);
        }
        if (!diagnostic.hook.empty())
        {
            where += std::format(" {}", diagnostic.hook);
        }
        if (diagnostic.actor.has_value())
        {
            where += std::format(", NPC {}", diagnostic.actor->value);
        }
        const bool error = diagnostic.kind == LuaScriptDiagnosticKind::Error;
        log.write(
            error ? ConsoleLevel::Error : ConsoleLevel::Info,
            std::format("[lua] {}{}: {}", where, error ? " error" : "", diagnostic.message));
    }
}
