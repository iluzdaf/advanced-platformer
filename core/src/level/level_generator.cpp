#include "advanced_platformer/level/level_generator.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "advanced_platformer/level/placements.hpp"
#include "advanced_platformer/level/room_pieces.hpp"
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

        struct LevelRandom
        {
            std::uint64_t state = 0;
        };

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

        struct RoomChoice
        {
            std::size_t piece = 0;
            bool mirrored = false;
        };
    }

    namespace
    {
        constexpr int LayoutAttempts = 200;
        constexpr int GrowthTriesPerRoom = 64;

        bool roomAt(const std::vector<RoomSlot>& rooms, Cell grid)
        {
            return std::ranges::any_of(
                rooms, [grid](const RoomSlot& room) { return room.grid == grid; });
        }

        int placedNeighbours(const std::vector<RoomSlot>& rooms, Cell grid)
        {
            return static_cast<int>(std::ranges::count_if(
                AllSides, [&](RoomSide side) { return roomAt(rooms, stepTowards(grid, side)); }));
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
                if (!contains(grid, next) || roomAt(rooms, next) ||
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
                    "Could not grow {} rooms in a {} by {} grid",
                    roomCount,
                    grid.width,
                    grid.height));
        }

        std::string doorNames(RoomDoors doors)
        {
            std::string result;
            for (const RoomSide side : AllSides)
            {
                if (hasDoor(doors, side))
                {
                    result += result.empty() ? "" : ", ";
                    result += nameOf(side);
                }
            }
            return result;
        }

        std::string_view roleNeeded(std::size_t room, const RoomLayout& layout)
        {
            if (room == 0)
            {
                return "start";
            }
            if (room == layout.exit)
            {
                return "exit";
            }
            return "corridor, shaft or arena";
        }

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
                    if (candidate.doors == doors)
                    {
                        candidates.push_back({piece, false});
                    }
                    if (candidate.mirror && mirroredDoors(candidate.doors) == doors)
                    {
                        candidates.push_back({piece, true});
                    }
                }
                if (candidates.empty())
                {
                    throw std::invalid_argument(
                        std::format(
                            "no {} piece has doors {}",
                            roleNeeded(room, layout),
                            doorNames(doors)));
                }
                choices.push_back(candidates[randomBelow(random, candidates.size())]);
            }
            return choices;
        }

        std::string roomName(std::size_t room)
        {
            return std::format("room{}", room);
        }

        GeneratedLevel stitchRooms(
            const RoomPieceCatalog& catalog,
            const RoomLayout& layout,
            const std::vector<RoomChoice>& choices)
        {
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

            const char solid = catalog.pieces.at(choices.front().piece).rows.front().front();
            GeneratedLevel level;
            level.tileLegend = catalog.tileLegend;
            level.mapRows.assign(
                static_cast<std::size_t>(height),
                std::string(static_cast<std::size_t>(width), solid));

            for (std::size_t room = 0; room < layout.rooms.size(); ++room)
            {
                const RoomSlot& slot = layout.rooms[room];
                const RoomChoice choice = choices[room];
                const RoomPiece& piece = catalog.pieces.at(choice.piece);
                const Cell origin{
                    (slot.grid.x - least.x) * stride.width,
                    (slot.grid.y - least.y) * stride.height};
                const auto placed = [&](Cell cell)
                {
                    const int column = choice.mirrored ? size.width - 1 - cell.x : cell.x;
                    return Cell{origin.x + column, origin.y + cell.y};
                };
                for (int row = 0; row < size.height; ++row)
                {
                    for (int column = 0; column < size.width; ++column)
                    {
                        const int source = choice.mirrored ? size.width - 1 - column : column;
                        const char symbol = piece.rows[static_cast<std::size_t>(row)]
                                                      [static_cast<std::size_t>(source)];
                        const Cell cell{origin.x + column, origin.y + row};
                        level.mapRows[static_cast<std::size_t>(cell.y)]
                                     [static_cast<std::size_t>(cell.x)] = symbol;
                    }
                }
                if (piece.playerSpawn.has_value())
                {
                    level.playerSpawn = placed(*piece.playerSpawn);
                }
                if (piece.exit.has_value())
                {
                    level.exit = *piece.exit;
                    level.exit.spawn = placed(piece.exit->spawn);
                }
                for (ActorPlacement actor : piece.actors)
                {
                    actor.id = std::format("{}_{}", roomName(room), actor.id);
                    actor.spawn = placed(actor.spawn);
                    if (actor.patrol.has_value())
                    {
                        actor.patrol = PatrolPlacement{
                            placed(actor.patrol->first), placed(actor.patrol->second)};
                    }
                    level.actors.push_back(std::move(actor));
                }
                for (PickupPlacement pickup : piece.pickups)
                {
                    pickup.id = std::format("{}_{}", roomName(room), pickup.id);
                    pickup.spawn = placed(pickup.spawn);
                    level.pickups.push_back(std::move(pickup));
                }
            }
            return level;
        }

        int roomsForLevel(const RunSettings& run, int levelNumber)
        {
            if (levelNumber <= 0)
            {
                throw std::invalid_argument(
                    std::format("Level {}: level numbers start at 1", levelNumber));
            }
            const std::int64_t rooms =
                run.firstRooms + (static_cast<std::int64_t>(levelNumber - 1) * run.roomsPerLevel);
            return static_cast<int>(std::min<std::int64_t>(rooms, run.maxRooms));
        }
    }

    GeneratedLevel generateLevel(
        const RoomPieceCatalog& catalog,
        int levelNumber,
        std::uint32_t seed,
        std::string_view levelName)
    {
        LevelRandom random{seed};
        const RoomLayout layout =
            layoutRooms(catalog.run.grid, roomsForLevel(catalog.run, levelNumber), random);
        std::vector<RoomChoice> choices;
        try
        {
            choices = chooseRooms(catalog, layout, random);
        }
        catch (const std::invalid_argument& error)
        {
            throw std::invalid_argument(std::format("{}: {}", levelName, error.what()));
        }
        return stitchRooms(catalog, layout, choices);
    }

    std::uint32_t runLevelSeed(std::uint32_t runSeed, int levelNumber)
    {
        LevelRandom random{
            (static_cast<std::uint64_t>(runSeed) << 32U) | static_cast<std::uint32_t>(levelNumber)};
        return static_cast<std::uint32_t>(nextRandom(random) >> 32U);
    }

    std::uint32_t nextRunSeed(std::uint32_t runSeed)
    {
        LevelRandom random{runSeed};
        return static_cast<std::uint32_t>(nextRandom(random) >> 32U);
    }
}
