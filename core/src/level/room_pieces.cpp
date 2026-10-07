#include "advanced_platformer/level/room_pieces.hpp"

#include <cstdint>
#include <stdexcept>
#include <string_view>

#include "advanced_platformer/math/coordinates.hpp"

namespace advanced_platformer
{
    namespace
    {
        std::uint8_t bitOf(RoomSide side)
        {
            return static_cast<std::uint8_t>(1U << static_cast<unsigned>(side));
        }
    }

    bool hasDoor(RoomDoors doors, RoomSide side)
    {
        return (doors.bits & bitOf(side)) != 0U;
    }

    RoomDoors withDoor(RoomDoors doors, RoomSide side)
    {
        return {static_cast<std::uint8_t>(doors.bits | bitOf(side))};
    }

    RoomDoors mirroredDoors(RoomDoors doors)
    {
        RoomDoors result{static_cast<std::uint8_t>(
            doors.bits & static_cast<std::uint8_t>(bitOf(RoomSide::Up) | bitOf(RoomSide::Down)))};
        if (hasDoor(doors, RoomSide::Left))
        {
            result = withDoor(result, RoomSide::Right);
        }
        if (hasDoor(doors, RoomSide::Right))
        {
            result = withDoor(result, RoomSide::Left);
        }
        return result;
    }

    RoomSide oppositeOf(RoomSide side)
    {
        switch (side)
        {
        case RoomSide::Left:
            return RoomSide::Right;
        case RoomSide::Right:
            return RoomSide::Left;
        case RoomSide::Up:
            return RoomSide::Down;
        case RoomSide::Down:
            return RoomSide::Up;
        }
        throw std::invalid_argument("Unknown room side");
    }

    Cell stepTowards(Cell cell, RoomSide side)
    {
        switch (side)
        {
        case RoomSide::Left:
            return {cell.x - 1, cell.y};
        case RoomSide::Right:
            return {cell.x + 1, cell.y};
        case RoomSide::Up:
            return {cell.x, cell.y - 1};
        case RoomSide::Down:
            return {cell.x, cell.y + 1};
        }
        throw std::invalid_argument("Unknown room side");
    }

    std::string_view nameOf(RoomSide side)
    {
        switch (side)
        {
        case RoomSide::Left:
            return "left";
        case RoomSide::Right:
            return "right";
        case RoomSide::Up:
            return "up";
        case RoomSide::Down:
            return "down";
        }
        return "unknown";
    }
}
