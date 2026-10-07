#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "advanced_platformer/math/coordinates.hpp"

#include "content/placements.hpp"
#include "content/room_pieces.hpp"

namespace advanced_platformer
{
    struct LevelData
    {
        std::map<char, std::string> tileLegend;
        std::vector<std::string> mapRows;
        Cell playerSpawn;
        std::vector<ActorPlacement> actors;
        std::vector<PickupPlacement> pickups;
        ExitPlacement exit;
    };

    struct LevelGeneration
    {
        GridSize grid;
        int roomCount = 0;
        std::uint32_t seed = 0;
    };

    LevelData generateLevel(
        const RoomPieceCatalog& catalog,
        const LevelGeneration& generation,
        std::string_view sourceName);

    LevelGeneration levelGeneration(const RunSettings& run, int levelNumber, std::uint32_t seed);
    std::uint32_t runLevelSeed(std::uint32_t runSeed, int levelNumber);
    std::uint32_t nextRunSeed(std::uint32_t runSeed);
}
