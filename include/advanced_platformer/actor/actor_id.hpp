#pragma once

#include <cstdint>

namespace advanced_platformer
{
    struct ActorId
    {
        std::uint32_t value = 0;

        bool operator==(const ActorId&) const = default;
    };

    bool isValid(ActorId id);
}
