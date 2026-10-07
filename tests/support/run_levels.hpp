#pragma once

#include "content/room_pieces.hpp"

namespace tests
{
    inline int levelsUntilCap(const advanced_platformer::RunSettings& run)
    {
        if (run.roomsPerLevel <= 0 || run.firstRooms >= run.maxRooms)
        {
            return 1;
        }
        const int growth = run.maxRooms - run.firstRooms;
        return 1 + ((growth + run.roomsPerLevel - 1) / run.roomsPerLevel);
    }
}
