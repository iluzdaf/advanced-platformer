#pragma once

#include <optional>
#include <string>

#include "advanced_platformer/math/coordinates.hpp"

namespace advanced_platformer
{
    struct NamedItemStack
    {
        std::string item;
        int quantity = 1;
    };

    struct PatrolPlacement
    {
        Cell first;
        Cell second;
    };

    struct ActorPlacement
    {
        std::string id;
        std::string definitionName;
        Cell spawn;
        std::optional<PatrolPlacement> patrol;
    };

    struct PickupPlacement
    {
        std::string id;
        std::string definitionName;
        Cell spawn;
    };

    struct ExitPlacement
    {
        std::string definitionName;
        Cell spawn;
        std::optional<NamedItemStack> requirement;
        bool consumeItem = false;
    };
}
