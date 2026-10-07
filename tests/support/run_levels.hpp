#pragma once

#include "content/room_pieces.hpp"
#include "game/level_generator.hpp"

namespace tests
{
    inline int levelsUntilCap(const advanced_platformer::RunSettings& run)
    {
        int number = 1;
        while (advanced_platformer::levelGeneration(run, number, 0).roomCount < run.maxRooms)
        {
            ++number;
        }
        return number;
    }
}
