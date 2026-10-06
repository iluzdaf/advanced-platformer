#pragma once

#include "content/run_settings.hpp"

namespace tests
{
    inline int levelsUntilCap(const advanced_platformer::RunSettings& run)
    {
        int number = 1;
        while (advanced_platformer::roomsForLevel(run, number) < run.maxRooms)
        {
            ++number;
        }
        return number;
    }
}
