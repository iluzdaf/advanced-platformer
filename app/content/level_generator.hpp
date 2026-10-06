#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "advanced_platformer/math/coordinates.hpp"

#include "level_data.hpp"
#include "room_pieces.hpp"

namespace advanced_platformer
{
    struct LevelGeneration
    {
        GridSize grid;
        int roomCount = 0;
        std::uint32_t seed = 0;
    };

    struct LevelRandom
    {
        std::uint64_t state = 0;
    };

    std::uint64_t nextRandom(LevelRandom& random);
    std::size_t randomBelow(LevelRandom& random, std::size_t count);

    struct RoomSlot
    {
        Cell grid;
        RoomDoors doors;
        int depth = 0;
    };

    struct RoomLayout
    {
        GridSize grid;
        std::vector<RoomSlot> rooms;
        std::size_t exit = 0;
    };

    RoomLayout layoutRooms(GridSize grid, int roomCount, LevelRandom& random);

    struct RoomChoice
    {
        std::size_t piece = 0;
        bool mirrored = false;
    };

    std::vector<RoomChoice> chooseRooms(
        const RoomPieceCatalog& catalog,
        const RoomLayout& layout,
        LevelRandom& random);

    LevelData stitchRooms(
        const RoomPieceCatalog& catalog,
        const RoomLayout& layout,
        const std::vector<RoomChoice>& choices);

    LevelData generateLevel(
        const RoomPieceCatalog& catalog,
        const LevelGeneration& generation,
        std::string_view sourceName);

    int roomsForLevel(const RunSettings& run, int levelNumber);
    LevelGeneration levelGeneration(const RunSettings& run, int levelNumber, std::uint32_t seed);
    std::uint32_t runLevelSeed(std::uint32_t runSeed, int levelNumber);
    std::uint32_t nextRunSeed(std::uint32_t runSeed);
}
