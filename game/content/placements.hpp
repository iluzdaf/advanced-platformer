#pragma once

#include <optional>
#include <string>

#include "advanced_platformer/math/coordinates.hpp"

#include "item_catalog.hpp"

namespace advanced_platformer
{
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
