#include "level_generator.hpp"

#include "level_catalog.hpp"
#include "level_data.hpp"
#include "room_pieces.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "advanced_platformer/math/coordinates.hpp"

namespace advanced_platformer
{
    namespace
    {
        constexpr std::array AllSides{
            RoomSide::Left,
            RoomSide::Right,
            RoomSide::Up,
            RoomSide::Down};

        std::optional<std::size_t> roomAt(const std::vector<RoomSlot>& rooms, Cell grid)
        {
            const auto found = std::ranges::find_if(
                rooms, [grid](const RoomSlot& room) { return room.grid == grid; });
            if (found == rooms.end())
            {
                return std::nullopt;
            }
            return static_cast<std::size_t>(found - rooms.begin());
        }

        std::string roomName(std::size_t room)
        {
            return std::format("room{}", room);
        }
    }

    std::uint64_t nextRandom(LevelRandom& random)
    {
        random.state += 0x9E3779B97F4A7C15ULL;
        std::uint64_t mixed = random.state;
        mixed = (mixed ^ (mixed >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        mixed = (mixed ^ (mixed >> 27U)) * 0x94D049BB133111EBULL;
        return mixed ^ (mixed >> 31U);
    }

    std::size_t randomBelow(LevelRandom& random, std::size_t count)
    {
        if (count == 0)
        {
            throw std::invalid_argument("A random choice needs at least one option");
        }
        constexpr std::uint64_t Largest = std::numeric_limits<std::uint64_t>::max();
        const auto range = static_cast<std::uint64_t>(count);
        const std::uint64_t limit = Largest - (Largest % range);
        std::uint64_t value = nextRandom(random);
        while (value >= limit)
        {
            value = nextRandom(random);
        }
        return static_cast<std::size_t>(value % range);
    }

    namespace
    {
        constexpr int LayoutAttempts = 200;
        constexpr int GrowthTriesPerRoom = 64;

        int placedNeighbours(const std::vector<RoomSlot>& rooms, Cell grid)
        {
            return static_cast<int>(std::ranges::count_if(
                AllSides,
                [&](RoomSide side) { return roomAt(rooms, stepTowards(grid, side)).has_value(); }));
        }

        std::optional<std::vector<RoomSlot>> growRooms(
            GridSize grid,
            int roomCount,
            LevelRandom& random)
        {
            std::vector<RoomSlot> rooms{{.grid = {grid.width / 2, grid.height / 2}}};
            int tries = 0;
            while (std::cmp_less(rooms.size(), roomCount))
            {
                if (++tries > roomCount * GrowthTriesPerRoom)
                {
                    return std::nullopt;
                }
                const std::size_t parent = randomBelow(random, rooms.size());
                const RoomSide side = AllSides[randomBelow(random, AllSides.size())];
                const Cell next = stepTowards(rooms[parent].grid, side);
                if (!contains(grid, next) || roomAt(rooms, next).has_value() ||
                    placedNeighbours(rooms, next) != 1)
                {
                    continue;
                }
                rooms[parent].doors = withDoor(rooms[parent].doors, side);
                rooms.push_back(
                    {.grid = next,
                     .doors = withDoor(RoomDoors{}, oppositeOf(side)),
                     .depth = rooms[parent].depth + 1});
            }
            return rooms;
        }
    }

    RoomLayout layoutRooms(GridSize grid, int roomCount, LevelRandom& random)
    {
        if (roomCount < 2 || roomCount > grid.width * grid.height)
        {
            throw std::invalid_argument(
                std::format(
                    "A level needs from 2 to {} rooms, not {}",
                    grid.width * grid.height,
                    roomCount));
        }
        for (int attempt = 0; attempt < LayoutAttempts; ++attempt)
        {
            std::optional<std::vector<RoomSlot>> rooms = growRooms(grid, roomCount, random);
            if (!rooms.has_value())
            {
                continue;
            }
            RoomLayout layout{.grid = grid, .rooms = std::move(*rooms)};
            for (std::size_t room = 1; room < layout.rooms.size(); ++room)
            {
                if (layout.rooms[room].depth > layout.rooms[layout.exit].depth)
                {
                    layout.exit = room;
                }
            }
            return layout;
        }
        throw std::invalid_argument(
            std::format(
                "Could not grow {} rooms in a {} by {} grid", roomCount, grid.width, grid.height));
    }

    namespace
    {
        bool roleFits(RoomRole role, std::size_t room, const RoomLayout& layout)
        {
            if (room == 0)
            {
                return role == RoomRole::Start;
            }
            if (room == layout.exit)
            {
                return role == RoomRole::Exit;
            }
            return role != RoomRole::Start && role != RoomRole::Exit;
        }
    }

    std::vector<RoomChoice> chooseRooms(
        const RoomPieceCatalog& catalog,
        const RoomLayout& layout,
        LevelRandom& random)
    {
        std::vector<RoomChoice> choices;
        choices.reserve(layout.rooms.size());
        for (std::size_t room = 0; room < layout.rooms.size(); ++room)
        {
            const RoomDoors doors = layout.rooms[room].doors;
            std::vector<RoomChoice> candidates;
            for (std::size_t piece = 0; piece < catalog.pieces.size(); ++piece)
            {
                const RoomPiece& candidate = catalog.pieces[piece];
                if (!roleFits(candidate.role, room, layout))
                {
                    continue;
                }
                if (coversDoors(candidate.doors, doors))
                {
                    candidates.push_back({piece, false});
                }
                if (candidate.mirror && coversDoors(mirroredDoors(candidate.doors), doors))
                {
                    candidates.push_back({piece, true});
                }
            }
            if (candidates.empty())
            {
                throw std::invalid_argument(std::format("No room piece fits {}", roomName(room)));
            }
            choices.push_back(candidates[randomBelow(random, candidates.size())]);
        }
        return choices;
    }

    LevelData stitchRooms(
        const RoomPieceCatalog& catalog,
        const RoomLayout& layout,
        const std::vector<RoomChoice>& choices,
        std::optional<int> nextLevel,
        std::string_view sourceName)
    {
        if (choices.size() != layout.rooms.size())
        {
            throw std::invalid_argument("Every room needs exactly one piece");
        }
        Cell least = layout.rooms.front().grid;
        Cell most = least;
        for (const RoomSlot& room : layout.rooms)
        {
            least = {std::min(least.x, room.grid.x), std::min(least.y, room.grid.y)};
            most = {std::max(most.x, room.grid.x), std::max(most.y, room.grid.y)};
        }
        const GridSize size = catalog.roomSize;
        const GridSize stride{size.width - 1, size.height - 1};
        const int width = ((most.x - least.x) * stride.width) + size.width;
        const int height = ((most.y - least.y) * stride.height) + size.height;

        LevelData level;
        level.tileLegend = catalog.tileLegend;
        level.mapRows.assign(
            static_cast<std::size_t>(height),
            std::string(static_cast<std::size_t>(width), catalog.wall));
        level.exit.definitionName = catalog.exitDefinition;
        level.exit.nextLevel = nextLevel;

        for (std::size_t room = 0; room < layout.rooms.size(); ++room)
        {
            const RoomSlot& slot = layout.rooms[room];
            const RoomChoice choice = choices[room];
            const RoomPiece& piece = catalog.pieces.at(choice.piece);
            const RoomDoors pieceDoors = choice.mirrored ? mirroredDoors(piece.doors) : piece.doors;
            if (!coversDoors(pieceDoors, slot.doors))
            {
                throw std::invalid_argument(
                    std::format(
                        "Room piece '{}' lacks a door that {} needs", piece.name, roomName(room)));
            }
            std::vector<Cell> sealed;
            for (const RoomSide side : AllSides)
            {
                if (hasDoor(pieceDoors, side) && !hasDoor(slot.doors, side))
                {
                    const std::vector<Cell> cells = doorCells(size, side);
                    sealed.insert(sealed.end(), cells.begin(), cells.end());
                }
            }

            const Cell origin{
                (slot.grid.x - least.x) * stride.width, (slot.grid.y - least.y) * stride.height};
            std::map<std::string, int> counts;
            for (int row = 0; row < size.height; ++row)
            {
                for (int column = 0; column < size.width; ++column)
                {
                    const int source = choice.mirrored ? size.width - 1 - column : column;
                    char symbol =
                        piece.rows[static_cast<std::size_t>(row)][static_cast<std::size_t>(source)];
                    const Cell cell{origin.x + column, origin.y + row};
                    if (std::ranges::contains(sealed, Cell{column, row}))
                    {
                        symbol = catalog.wall;
                    }
                    else if (symbol == catalog.startMarker)
                    {
                        level.playerSpawn = cell;
                        symbol = catalog.open;
                    }
                    else if (symbol == catalog.exitMarker)
                    {
                        level.exit.spawn = cell;
                        symbol = catalog.open;
                    }
                    else if (
                        const auto actor = catalog.actorMarkers.find(symbol);
                        actor != catalog.actorMarkers.end())
                    {
                        const int count = ++counts[actor->second];
                        level.actors.push_back(
                            {.id = std::format("{}_{}_{}", roomName(room), actor->second, count),
                             .definitionName = actor->second,
                             .spawn = cell});
                        symbol = catalog.open;
                    }
                    else if (
                        const auto pickup = catalog.pickupMarkers.find(symbol);
                        pickup != catalog.pickupMarkers.end())
                    {
                        const int count = ++counts[pickup->second];
                        level.pickups.push_back(
                            {.id = std::format("{}_{}_{}", roomName(room), pickup->second, count),
                             .definitionName = pickup->second,
                             .spawn = cell});
                        symbol = catalog.open;
                    }
                    level.mapRows[static_cast<std::size_t>(cell.y)]
                                 [static_cast<std::size_t>(cell.x)] = symbol;
                }
            }
        }
        return parseLevelData(formatLevelData(level), sourceName);
    }

    LevelData generateLevel(
        const RoomPieceCatalog& catalog,
        const LevelGeneration& generation,
        std::string_view sourceName)
    {
        LevelRandom random{generation.seed};
        const RoomLayout layout = layoutRooms(generation.grid, generation.roomCount, random);
        const std::vector<RoomChoice> choices = chooseRooms(catalog, layout, random);
        return stitchRooms(catalog, layout, choices, generation.nextLevel, sourceName);
    }
}
