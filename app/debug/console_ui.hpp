#pragma once

#include <cstddef>

namespace advanced_platformer
{
    class ConsoleLog;

    struct ConsoleView
    {
        std::size_t shownWritten = 0;
    };

    void drawConsole(ConsoleView& view, const ConsoleLog& log);
}
