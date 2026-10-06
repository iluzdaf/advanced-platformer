#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "advanced_platformer/math/coordinates.hpp"

#include "level_catalog.hpp"
#include "level_data.hpp"
#include "room_pieces.hpp"

namespace advanced_platformer
{
    // A small, fast random number generator (SplitMix64) whose numbers are the same on
    // every platform and standard library, so a seed builds the same level everywhere.
    struct LevelRandom
    {
        std::uint64_t state = 0;
    };

    std::uint64_t nextRandom(LevelRandom& random);
    // A whole number from 0 up to, but not including, count, which is positive.
    std::size_t randomBelow(LevelRandom& random, std::size_t count);

    struct RoomSlot
    {
        // The slot in the room grid.
        Cell grid;
        RoomDoors doors;
        // How many doors lie between it and the start room.
        int depth = 0;
    };

    // Rooms joined into a tree by their doors. The first room is the start, and the exit is
    // the room farthest from it, always a dead end.
    struct RoomLayout
    {
        GridSize grid;
        std::vector<RoomSlot> rooms;
        std::size_t exit = 0;
    };

    // Grows roomCount rooms from the centre of the grid. Each new room opens off one room
    // already placed and touches no other, so the rooms form branching corridors without
    // loops.
    RoomLayout layoutRooms(GridSize grid, int roomCount, LevelRandom& random);

    struct RoomChoice
    {
        std::size_t piece = 0;
        bool mirrored = false;
    };

    // A piece for each room whose role suits the room and whose doors include the room's.
    std::vector<RoomChoice> chooseRooms(
        const RoomPieceCatalog& catalog,
        const RoomLayout& layout,
        LevelRandom& random);

    // Lays the chosen pieces out on one map, neighbours sharing the wall between them,
    // seals the doors each room does not use, fills empty slots with wall, and turns the
    // pieces' markers into placements. The level is read back through parseLevelData, so
    // it passes the same checks as a level file.
    LevelData stitchRooms(
        const RoomPieceCatalog& catalog,
        const RoomLayout& layout,
        const std::vector<RoomChoice>& choices,
        std::optional<int> nextLevel,
        std::string_view sourceName);

    LevelData generateLevel(
        const RoomPieceCatalog& catalog,
        const LevelGeneration& generation,
        std::string_view sourceName);
}
