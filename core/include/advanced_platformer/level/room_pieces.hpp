#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "advanced_platformer/level/placements.hpp"
#include "advanced_platformer/math/coordinates.hpp"

namespace advanced_platformer
{
    enum class RoomSide : std::uint8_t
    {
        Left,
        Right,
        Up,
        Down
    };

    struct RoomDoors
    {
        std::uint8_t bits = 0;

        bool operator==(const RoomDoors&) const = default;
    };

    bool hasDoor(RoomDoors doors, RoomSide side);
    RoomDoors withDoor(RoomDoors doors, RoomSide side);
    RoomDoors mirroredDoors(RoomDoors doors);

    RoomSide oppositeOf(RoomSide side);
    Cell stepTowards(Cell cell, RoomSide side);
    std::string_view nameOf(RoomSide side);

    enum class RoomRole : std::uint8_t
    {
        Start,
        Exit,
        Corridor,
        Shaft,
        Arena
    };

    struct RoomPiece
    {
        std::string name;
        RoomRole role = RoomRole::Corridor;
        RoomDoors doors;
        std::vector<std::string> rows;
        bool mirror = true;
        std::optional<Cell> playerSpawn;
        std::optional<ExitPlacement> exit;
        std::vector<ActorPlacement> actors;
        std::vector<PickupPlacement> pickups;
    };

    struct RunSettings
    {
        GridSize grid;
        int firstRooms = 0;
        int roomsPerLevel = 0;
        int maxRooms = 0;
    };

    struct RoomPieceCatalog
    {
        GridSize roomSize;
        RunSettings run;
        std::map<char, std::string> tileLegend;
        char open = '.';
        std::vector<RoomPiece> pieces;
    };
}
