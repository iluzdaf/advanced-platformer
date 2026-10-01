#pragma once

#include "advanced_platformer/timing/fixed_step.hpp"

namespace tests
{
    // The game's fixed simulation step, as the float the update functions take.
    constexpr float FixedStepSeconds = static_cast<float>(advanced_platformer::FixedDeltaSeconds);
}
