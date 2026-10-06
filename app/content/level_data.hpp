#pragma once

#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/math/coordinates.hpp"

#include "item_catalog.hpp"

namespace advanced_platformer
{
    using LevelPosition = std::variant<Cell, glm::vec2>;

    struct PatrolPlacement
    {
        LevelPosition first;
        LevelPosition second;
    };

    struct ActorPlacement
    {
        std::string id;
        std::string definitionName;
        LevelPosition spawn;
        std::optional<PatrolPlacement> patrol;
    };

    struct PickupPlacement
    {
        std::string id;
        std::string definitionName;
        LevelPosition spawn;
    };

    struct ExitPlacement
    {
        std::string definitionName;
        LevelPosition spawn;
        std::optional<NamedItemStack> requirement;
        bool consumeItem = false;
    };

    struct LevelData
    {
        std::map<char, std::string> tileLegend;
        std::vector<std::string> mapRows;
        LevelPosition playerSpawn;
        std::vector<ActorPlacement> actors;
        std::vector<PickupPlacement> pickups;
        ExitPlacement exit;
    };
}
