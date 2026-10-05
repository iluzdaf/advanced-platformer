#pragma once

#include <cstddef>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "advanced_platformer/input/input_program.hpp"

namespace tests
{
    inline void requireSameInputProgram(
        const advanced_platformer::InputProgram& left,
        const advanced_platformer::InputProgram& right)
    {
        REQUIRE(left.size() == right.size());
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            CAPTURE(index);
            REQUIRE(left[index] == right[index]);
        }
    }
}
