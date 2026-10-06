#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

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

    // A set of room sides, one bit per RoomSide.
    struct RoomDoors
    {
        std::uint8_t bits = 0;

        bool operator==(const RoomDoors&) const = default;
    };

    bool hasDoor(RoomDoors doors, RoomSide side);
    RoomDoors withDoor(RoomDoors doors, RoomSide side);
    // Whether every door of other is also in doors.
    bool coversDoors(RoomDoors doors, RoomDoors other);
    int doorCount(RoomDoors doors);
    // Left and right swapped, as a room flipped left to right has them.
    RoomDoors mirroredDoors(RoomDoors doors);

    RoomSide oppositeOf(RoomSide side);
    Cell stepTowards(Cell cell, RoomSide side);
    // "left", "right", "up" or "down".
    std::string_view nameOf(RoomSide side);

    // What a room is for. A level starts in a start room and ends in an exit room. The rest
    // fill the path between them and its branches.
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
        // roomSize rows of map symbols and marker symbols.
        std::vector<std::string> rows;
        // Whether the generator may flip it left to right.
        bool mirror = true;
    };

    // Hand-authored rooms that the generator stitches into levels, with the symbols they
    // share. Every piece is the same size, so any piece fits any slot of the room grid,
    // and its doors sit at the same places on its edges, so neighbours' doors line up.
    struct RoomPieceCatalog
    {
        GridSize roomSize;
        std::map<char, std::string> tileLegend;
        // Seals unused doors and fills grid slots that hold no room.
        char wall = '#';
        // Every door opening, and what a marker leaves behind.
        char open = '.';
        char startMarker = 'S';
        char exitMarker = 'E';
        std::map<char, std::string> actorMarkers;
        std::map<char, std::string> pickupMarkers;
        std::string exitDefinition;
        std::vector<RoomPiece> pieces;
    };

    // The cells of the door on that side of a room, relative to the room's top-left cell.
    // Side doors are three cells tall and stand on the bottom row; doors above and below
    // are four cells wide and centred.
    std::vector<Cell> doorCells(GridSize roomSize, RoomSide side);

    RoomPieceCatalog parseRoomPieceCatalog(std::string_view text, std::string_view sourceName);
    RoomPieceCatalog loadRoomPieceCatalog(const std::filesystem::path& path);
}
