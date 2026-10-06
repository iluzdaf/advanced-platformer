#pragma once

#include "game/level_generator.hpp"
#include "content/room_pieces.hpp"

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
