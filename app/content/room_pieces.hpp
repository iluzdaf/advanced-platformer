#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "advanced_platformer/math/coordinates.hpp"

#include "item_catalog.hpp"

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
    bool coversDoors(RoomDoors doors, RoomDoors other);
    int doorCount(RoomDoors doors);
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
    };

    struct RoomPieceCatalog
    {
        GridSize roomSize;
        std::map<char, std::string> tileLegend;
        char wall = '#';
        char open = '.';
        char startMarker = 'S';
        char exitMarker = 'E';
        std::map<char, std::string> actorMarkers;
        std::map<char, std::string> pickupMarkers;
        std::string exitDefinition;
        std::optional<NamedItemStack> exitRequirement;
        bool consumeExitItem = false;
        std::vector<RoomPiece> pieces;
    };

    std::vector<Cell> doorCells(GridSize roomSize, RoomSide side);

    RoomPieceCatalog parseRoomPieceCatalog(std::string_view text, std::string_view sourceName);
    RoomPieceCatalog loadRoomPieceCatalog(const std::filesystem::path& path);
}
