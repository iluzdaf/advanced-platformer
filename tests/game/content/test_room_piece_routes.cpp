#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>

#include "content/game_catalogs.hpp"
#include "advanced_platformer/level/room_pieces.hpp"
#include "content/room_pieces.hpp"
#include "advanced_platformer/level/level_generator.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "support/player_reach.hpp"

namespace
{
    using advanced_platformer::Cell;
    using advanced_platformer::GeneratedLevel;
    using advanced_platformer::RoomDoors;
    using advanced_platformer::RoomPiece;
    using advanced_platformer::RoomPieceCatalog;
    using advanced_platformer::RoomRole;
    using advanced_platformer::RoomSide;
    constexpr std::array AllSides{RoomSide::Left, RoomSide::Right, RoomSide::Up, RoomSide::Down};
    constexpr advanced_platformer::GridSize Grid{5, 5};
    constexpr Cell Centre{2, 2};
    constexpr std::uint32_t SeedsToTry = 50000;

    struct Orientation
    {
        RoomPiece piece;
        bool mirrored = false;
    };

    RoomDoors doorsOf(const Orientation& orientation)
    {
        const RoomDoors doors = orientation.piece.doors;
        return orientation.mirrored ? mirroredDoors(doors) : doors;
    }

    std::string describe(const Orientation& orientation)
    {
        return orientation.piece.name + (orientation.mirrored ? " (mirrored)" : "");
    }

    bool mirrorsToItself(const RoomPiece& piece)
    {
        return std::ranges::all_of(
            piece.rows,
            [](const std::string& row)
            { return std::ranges::equal(row, row | std::views::reverse); });
    }

    std::vector<Orientation> orientations(const RoomPieceCatalog& catalog, RoomRole role)
    {
        std::vector<Orientation> result;
        for (const RoomPiece& piece : catalog.pieces)
        {
            if (piece.role != role)
            {
                continue;
            }
            result.push_back({piece, false});
            if (piece.mirror && !mirrorsToItself(piece))
            {
                result.push_back({piece, true});
            }
        }
        return result;
    }

    bool isMirroredAt(const GeneratedLevel& level, const RoomPiece& piece, Cell origin)
    {
        for (std::size_t row = 0; row < piece.rows.size(); ++row)
        {
            const std::string placed =
                level.mapRows[static_cast<std::size_t>(origin.y) + row].substr(
                    static_cast<std::size_t>(origin.x), piece.rows[row].size());
            if (placed != piece.rows[row])
            {
                return true;
            }
        }
        return false;
    }

    struct Route
    {
        RoomPieceCatalog catalog;
        std::vector<Cell> slots;
        Orientation tested;
        Cell testedSlot;
    };

    Cell originOf(const RoomPieceCatalog& catalog, const Route& route)
    {
        Cell least = route.slots.front();
        for (const Cell slot : route.slots)
        {
            least = {std::min(least.x, slot.x), std::min(least.y, slot.y)};
        }
        return {
            (route.testedSlot.x - least.x) * (catalog.roomSize.width - 1),
            (route.testedSlot.y - least.y) * (catalog.roomSize.height - 1)};
    }

    std::optional<GeneratedLevel> generateRoute(Route route)
    {
        const int rooms = static_cast<int>(route.slots.size());
        route.catalog.run = {
            .grid = Grid, .firstRooms = rooms, .roomsPerLevel = 0, .maxRooms = rooms};
        for (std::uint32_t seed = 1; seed <= SeedsToTry; ++seed)
        {
            GeneratedLevel level;
            try
            {
                level = advanced_platformer::generateLevel(route.catalog, 1, seed, "route");
            }
            catch (const std::invalid_argument&)
            {
                continue;
            }
            if (isMirroredAt(level, route.tested.piece, originOf(route.catalog, route)) ==
                route.tested.mirrored)
            {
                return level;
            }
        }
        return std::nullopt;
    }

    bool routeWorks(const Route& route)
    {
        static const advanced_platformer::GameCatalogs shippedCatalogs =
            advanced_platformer::loadGameCatalogs("assets/catalogs");
        const std::optional<GeneratedLevel> level = generateRoute(route);
        if (!level.has_value())
        {
            FAIL("No seed laid the route out");
            return false;
        }
        return tests::playerReachesExit(*level, shippedCatalogs);
    }

    void addPiecesWithOnlyDoor(
        RoomPieceCatalog& catalog,
        const RoomPieceCatalog& shipped,
        RoomRole role,
        RoomSide side)
    {
        for (const RoomPiece& piece : shipped.pieces)
        {
            if (piece.role == role &&
                (piece.doors == withDoor(RoomDoors{}, side) ||
                 (piece.mirror && mirroredDoors(piece.doors) == withDoor(RoomDoors{}, side))))
            {
                catalog.pieces.push_back(piece);
            }
        }
    }

    void addFillersWithOnlyDoor(
        RoomPieceCatalog& catalog,
        const RoomPieceCatalog& shipped,
        RoomSide side)
    {
        for (const RoomRole role : {RoomRole::Corridor, RoomRole::Shaft, RoomRole::Arena})
        {
            addPiecesWithOnlyDoor(catalog, shipped, role, side);
        }
    }

    RoomPieceCatalog emptyCatalog(const RoomPieceCatalog& shipped)
    {
        return {
            .roomSize = shipped.roomSize,
            .run = shipped.run,
            .tileLegend = shipped.tileLegend,
            .open = shipped.open};
    }
}

TEST_CASE("Every shipped room piece joins each pair of its doors", "[app][content][generation]")
{
    const RoomPieceCatalog shipped =
        advanced_platformer::loadRoomPieceCatalog("assets/catalogs/pieces.json");
    for (const RoomRole role : {RoomRole::Corridor, RoomRole::Shaft, RoomRole::Arena})
    {
        for (const Orientation& piece : orientations(shipped, role))
        {
            const RoomDoors doors = doorsOf(piece);
            for (const RoomSide from : AllSides)
            {
                for (const RoomSide to : AllSides)
                {
                    if (from == to || !hasDoor(doors, from) || !hasDoor(doors, to))
                    {
                        continue;
                    }
                    const RoomSide towardsPiece = advanced_platformer::oppositeOf(from);
                    const Cell pieceSlot = advanced_platformer::stepTowards(Centre, towardsPiece);
                    Route route{
                        .catalog = emptyCatalog(shipped),
                        .slots = {Centre, pieceSlot},
                        .tested = piece,
                        .testedSlot = pieceSlot};
                    addPiecesWithOnlyDoor(route.catalog, shipped, RoomRole::Start, towardsPiece);
                    route.catalog.pieces.push_back(piece.piece);
                    for (const RoomSide door : AllSides)
                    {
                        if (door == from || !hasDoor(doors, door))
                        {
                            continue;
                        }
                        route.slots.push_back(advanced_platformer::stepTowards(pieceSlot, door));
                        const RoomSide facing = advanced_platformer::oppositeOf(door);
                        if (door == to)
                        {
                            addPiecesWithOnlyDoor(route.catalog, shipped, RoomRole::Exit, facing);
                        }
                        else
                        {
                            addFillersWithOnlyDoor(route.catalog, shipped, facing);
                        }
                    }
                    INFO(
                        describe(piece) << " from " << advanced_platformer::nameOf(from) << " to "
                                        << advanced_platformer::nameOf(to));
                    CHECK(routeWorks(route));
                }
            }
        }
    }
}

TEST_CASE(
    "Every shipped start room reaches an exit through each of its doors",
    "[app][content][generation]")
{
    const RoomPieceCatalog shipped =
        advanced_platformer::loadRoomPieceCatalog("assets/catalogs/pieces.json");
    for (const Orientation& piece : orientations(shipped, RoomRole::Start))
    {
        const RoomDoors doors = doorsOf(piece);
        for (const RoomSide side : AllSides)
        {
            if (!hasDoor(doors, side))
            {
                continue;
            }
            Route route{
                .catalog = emptyCatalog(shipped),
                .slots = {Centre},
                .tested = piece,
                .testedSlot = Centre};
            route.catalog.pieces.push_back(piece.piece);
            for (const RoomSide door : AllSides)
            {
                if (!hasDoor(doors, door))
                {
                    continue;
                }
                route.slots.push_back(advanced_platformer::stepTowards(Centre, door));
                const RoomSide facing = advanced_platformer::oppositeOf(door);
                if (door == side)
                {
                    addPiecesWithOnlyDoor(route.catalog, shipped, RoomRole::Exit, facing);
                }
                else
                {
                    addFillersWithOnlyDoor(route.catalog, shipped, facing);
                }
            }
            INFO(describe(piece) << " through " << advanced_platformer::nameOf(side));
            CHECK(routeWorks(route));
        }
    }
}

TEST_CASE("Every shipped exit room is reached through its door", "[app][content][generation]")
{
    const RoomPieceCatalog shipped =
        advanced_platformer::loadRoomPieceCatalog("assets/catalogs/pieces.json");
    for (const Orientation& piece : orientations(shipped, RoomRole::Exit))
    {
        const RoomDoors doors = doorsOf(piece);
        for (const RoomSide side : AllSides)
        {
            if (!hasDoor(doors, side))
            {
                continue;
            }
            const RoomSide towardsExit = advanced_platformer::oppositeOf(side);
            const Cell exitSlot = advanced_platformer::stepTowards(Centre, towardsExit);
            Route route{
                .catalog = emptyCatalog(shipped),
                .slots = {Centre, exitSlot},
                .tested = piece,
                .testedSlot = exitSlot};
            addPiecesWithOnlyDoor(route.catalog, shipped, RoomRole::Start, towardsExit);
            route.catalog.pieces.push_back(piece.piece);
            INFO(describe(piece) << " through " << advanced_platformer::nameOf(side));
            CHECK(routeWorks(route));
        }
    }
}
