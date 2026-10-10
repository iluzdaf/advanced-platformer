#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "content/game_catalogs.hpp"
#include "content/item_catalog.hpp"
#include "content/pickup_catalog.hpp"
#include "content/tile_catalog.hpp"
#include "level/level_composition.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/level/placements.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/level/room_pieces.hpp"
#include "content/room_pieces.hpp"
#include "advanced_platformer/level/level_generator.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/world/level_validation.hpp"
#include "advanced_platformer/world/pickup.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "support/fixed_step.hpp"
#include "support/player_reach.hpp"

namespace
{
    using advanced_platformer::Cell;
    using advanced_platformer::GeneratedLevel;
    using advanced_platformer::RoomDoors;
    using advanced_platformer::RoomPiece;
    using advanced_platformer::RoomPieces;
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

    std::vector<Orientation> orientations(const RoomPieces& catalog, RoomRole role)
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
        std::string name;
        RoomPieces catalog;
        std::vector<Cell> slots;
        Orientation tested;
        Cell testedSlot;
        Cell exitSlot;
        std::vector<std::pair<Cell, std::string>> pinnedRooms;
    };

    Cell originOf(const RoomPieces& catalog, const Route& route, Cell room)
    {
        Cell least = route.slots.front();
        for (const Cell slot : route.slots)
        {
            least = {std::min(least.x, slot.x), std::min(least.y, slot.y)};
        }
        return {
            (room.x - least.x) * (catalog.roomSize.width - 1),
            (room.y - least.y) * (catalog.roomSize.height - 1)};
    }

    bool pieceAt(
        const GeneratedLevel& level,
        const Route& route,
        Cell slot,
        const std::string& name)
    {
        const Cell origin = originOf(route.catalog, route, slot);
        return std::ranges::any_of(
            level.rooms,
            [&](const advanced_platformer::GeneratedRoom& room)
            { return room.origin == origin && room.piece == name; });
    }

    bool roomAt(const GeneratedLevel& level, const Route& route, Cell slot, RoomRole role)
    {
        const Cell origin = originOf(route.catalog, route, slot);
        return std::ranges::any_of(
            level.rooms,
            [&](const advanced_platformer::GeneratedRoom& room)
            {
                return room.origin == origin &&
                       std::ranges::any_of(
                           route.catalog.pieces,
                           [&](const RoomPiece& piece)
                           { return piece.name == room.piece && piece.role == role; });
            });
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
            if (roomAt(level, route, route.slots.front(), RoomRole::Start) &&
                roomAt(level, route, route.exitSlot, RoomRole::Exit) &&
                std::ranges::all_of(
                    route.pinnedRooms,
                    [&](const auto& pinned)
                    { return pieceAt(level, route, pinned.first, pinned.second); }) &&
                isMirroredAt(
                    level, route.tested.piece, originOf(route.catalog, route, route.testedSlot)) ==
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
        RoomPieces& catalog,
        const RoomPieces& shipped,
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

    void addFillersWithOnlyDoor(RoomPieces& catalog, const RoomPieces& shipped, RoomSide side)
    {
        for (const RoomRole role : {RoomRole::Corridor, RoomRole::Shaft, RoomRole::Arena})
        {
            addPiecesWithOnlyDoor(catalog, shipped, role, side);
        }
    }

    RoomPieces emptyCatalog(const RoomPieces& shipped)
    {
        return {
            .roomSize = shipped.roomSize,
            .run = shipped.run,
            .tileLegend = shipped.tileLegend,
            .open = shipped.open};
    }

    std::vector<Route> doorPairRoutes(const RoomPieces& shipped)
    {
        std::vector<Route> routes;
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
                        const Cell pieceSlot =
                            advanced_platformer::stepTowards(Centre, towardsPiece);
                        Route route{
                            .name = describe(piece) + " from " +
                                    std::string(advanced_platformer::nameOf(from)) + " to " +
                                    std::string(advanced_platformer::nameOf(to)),
                            .catalog = emptyCatalog(shipped),
                            .slots = {Centre, pieceSlot},
                            .tested = piece,
                            .testedSlot = pieceSlot};
                        addPiecesWithOnlyDoor(
                            route.catalog, shipped, RoomRole::Start, towardsPiece);
                        route.catalog.pieces.push_back(piece.piece);
                        for (const RoomSide door : AllSides)
                        {
                            if (door == from || !hasDoor(doors, door))
                            {
                                continue;
                            }
                            route.slots.push_back(
                                advanced_platformer::stepTowards(pieceSlot, door));
                            const RoomSide facing = advanced_platformer::oppositeOf(door);
                            if (door == to)
                            {
                                route.exitSlot = route.slots.back();
                                addPiecesWithOnlyDoor(
                                    route.catalog, shipped, RoomRole::Exit, facing);
                            }
                            else
                            {
                                addFillersWithOnlyDoor(route.catalog, shipped, facing);
                            }
                        }
                        routes.push_back(std::move(route));
                    }
                }
            }
        }
        return routes;
    }

    std::vector<Route> startRoutes(const RoomPieces& shipped)
    {
        std::vector<Route> routes;
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
                    .name = describe(piece) + " through " +
                            std::string(advanced_platformer::nameOf(side)),
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
                        route.exitSlot = route.slots.back();
                        addPiecesWithOnlyDoor(route.catalog, shipped, RoomRole::Exit, facing);
                    }
                    else
                    {
                        addFillersWithOnlyDoor(route.catalog, shipped, facing);
                    }
                }
                routes.push_back(std::move(route));
            }
        }
        return routes;
    }

    std::vector<Route> exitRoutes(const RoomPieces& shipped)
    {
        std::vector<Route> routes;
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
                    .name = describe(piece) + " through " +
                            std::string(advanced_platformer::nameOf(side)),
                    .catalog = emptyCatalog(shipped),
                    .slots = {Centre, exitSlot},
                    .tested = piece,
                    .testedSlot = exitSlot,
                    .exitSlot = exitSlot};
                addPiecesWithOnlyDoor(route.catalog, shipped, RoomRole::Start, towardsExit);
                route.catalog.pieces.push_back(piece.piece);
                routes.push_back(std::move(route));
            }
        }
        return routes;
    }

    RoomPieces shippedPieces()
    {
        return advanced_platformer::loadRoomPieceCatalog("assets/catalogs/pieces.json");
    }
}

TEST_CASE("Every shipped room piece joins each pair of its doors", "[app][content][generation]")
{
    for (const Route& route : doorPairRoutes(shippedPieces()))
    {
        INFO(route.name);
        CHECK(routeWorks(route));
    }
}

TEST_CASE(
    "Every shipped start room reaches an exit through each of its doors",
    "[app][content][generation]")
{
    for (const Route& route : startRoutes(shippedPieces()))
    {
        INFO(route.name);
        CHECK(routeWorks(route));
    }
}

namespace
{
    struct ReturnRoute
    {
        Route route;
        Cell leftSlot;
        RoomSide leftThrough = RoomSide::Left;
    };

    std::vector<ReturnRoute> startReturnRoutes(const RoomPieces& shipped)
    {
        std::vector<ReturnRoute> routes;
        for (const Route& route : startRoutes(shipped))
        {
            const RoomDoors doors = doorsOf(route.tested);
            for (const RoomSide side : AllSides)
            {
                const Cell slot = advanced_platformer::stepTowards(Centre, side);
                if (!hasDoor(doors, side) || slot == route.exitSlot)
                {
                    continue;
                }
                RoomPieces fillers = emptyCatalog(shipped);
                addFillersWithOnlyDoor(fillers, shipped, advanced_platformer::oppositeOf(side));
                for (const RoomPiece& filler : fillers.pieces)
                {
                    ReturnRoute returning{.route = route, .leftSlot = slot, .leftThrough = side};
                    returning.route.pinnedRooms = {{slot, filler.name}};
                    RoomPieces& catalog = returning.route.catalog;
                    catalog = emptyCatalog(shipped);
                    catalog.pieces.push_back(route.tested.piece);
                    for (const RoomSide door : AllSides)
                    {
                        const Cell next = advanced_platformer::stepTowards(Centre, door);
                        const RoomSide facing = advanced_platformer::oppositeOf(door);
                        if (!hasDoor(doors, door) || door == side)
                        {
                            continue;
                        }
                        if (next == route.exitSlot)
                        {
                            addPiecesWithOnlyDoor(catalog, shipped, RoomRole::Exit, facing);
                        }
                        else
                        {
                            addFillersWithOnlyDoor(catalog, shipped, facing);
                        }
                    }
                    if (std::ranges::none_of(
                            catalog.pieces,
                            [&](const RoomPiece& piece) { return piece.name == filler.name; }))
                    {
                        catalog.pieces.push_back(filler);
                    }
                    returning.route.name = route.name + " after leaving through " +
                                           std::string(advanced_platformer::nameOf(side)) +
                                           " into " + filler.name;
                    routes.push_back(std::move(returning));
                }
            }
        }
        return routes;
    }

    bool standable(const advanced_platformer::TileMap& map, Cell cell)
    {
        return !map.blocksMovement(cell) && !map.blocksMovement({cell.x, cell.y - 1}) &&
               map.blocksMovement({cell.x, cell.y + 1});
    }

    Cell doorway(
        const advanced_platformer::TileMap& map,
        const RoomPieces& catalog,
        const ReturnRoute& returning)
    {
        const Cell origin = originOf(catalog, returning.route, Centre);
        const int width = catalog.roomSize.width - 1;
        const int height = catalog.roomSize.height - 1;
        const bool sideways =
            returning.leftThrough == RoomSide::Left || returning.leftThrough == RoomSide::Right;
        const Cell first =
            returning.leftThrough == RoomSide::Right  ? Cell{origin.x + width, origin.y}
            : returning.leftThrough == RoomSide::Down ? Cell{origin.x, origin.y + height}
                                                      : origin;
        std::vector<Cell> open;
        for (int step = 0; step <= (sideways ? height : width); ++step)
        {
            const Cell cell =
                sideways ? Cell{first.x, first.y + step} : Cell{first.x + step, first.y};
            if (!map.blocksMovement(cell))
            {
                open.push_back(cell);
            }
        }
        if (sideways)
        {
            return open.back();
        }
        return open[open.size() / 2];
    }

    std::optional<Cell> firstLandingOutside(
        const GeneratedLevel& level,
        const ReturnRoute& returning,
        const advanced_platformer::GameCatalogs& catalogs)
    {
        const RoomPieces& catalog = returning.route.catalog;
        const advanced_platformer::TileMap map =
            advanced_platformer::composeTileMap(level.mapRows, level.tileLegend, catalogs.tiles);
        const Cell origin = originOf(catalog, returning.route, returning.leftSlot);
        const Cell door = doorway(map, catalog, returning);
        std::vector<Cell> cells;
        for (int y = origin.y + 1; y < origin.y + catalog.roomSize.height - 1; ++y)
        {
            for (int x = origin.x + 1; x < origin.x + catalog.roomSize.width - 1; ++x)
            {
                if (standable(map, {x, y}))
                {
                    cells.push_back({x, y});
                }
            }
        }
        std::ranges::sort(
            cells,
            {},
            [&](Cell cell)
            {
                const int dx = cell.x - door.x;
                const int dy = cell.y - door.y;
                return dx * dx + dy * dy;
            });
        advanced_platformer::Actor player = advanced_platformer::composePlayer(catalogs, 0);
        advanced_platformer::moveFeetTo(
            player.body.bounds, advanced_platformer::feetInCell(map.tileSize(), level.playerSpawn));
        for (const Cell cell : cells)
        {
            if (advanced_platformer::actorCanReach(
                    map,
                    player,
                    advanced_platformer::feetInCell(map.tileSize(), cell),
                    tests::FixedStepSeconds))
            {
                return cell;
            }
        }
        return std::nullopt;
    }
}

TEST_CASE(
    "Every shipped start room lets the player back in through each door they leave by",
    "[app][content][generation]")
{
    const advanced_platformer::GameCatalogs catalogs =
        advanced_platformer::loadGameCatalogs("assets/catalogs");
    for (const ReturnRoute& returning : startReturnRoutes(shippedPieces()))
    {
        INFO(returning.route.name);
        const std::optional<GeneratedLevel> level = generateRoute(returning.route);
        CHECK(level.has_value());
        if (!level.has_value())
        {
            continue;
        }
        const std::optional<Cell> landing = firstLandingOutside(*level, returning, catalogs);
        CHECK(landing.has_value());
        if (!landing.has_value())
        {
            continue;
        }
        GeneratedLevel outside = *level;
        outside.playerSpawn = *landing;
        CHECK(tests::playerReachesExit(outside, catalogs));
    }
}

TEST_CASE("Every shipped exit room is reached through its door", "[app][content][generation]")
{
    for (const Route& route : exitRoutes(shippedPieces()))
    {
        INFO(route.name);
        CHECK(routeWorks(route));
    }
}

namespace
{
    constexpr float SettleSeconds = 10.0F;

    glm::vec2 feetOnceTileBelowBreaks(
        const advanced_platformer::TileMap& map,
        const advanced_platformer::Pickup& pickup,
        const advanced_platformer::GameCatalogs& catalogs)
    {
        const glm::vec2 feet = advanced_platformer::feetOf(pickup.body.bounds);
        const Cell cell = advanced_platformer::cellAtFeet(map.tileSize(), feet);
        advanced_platformer::TileMap broken = map;
        if (!broken.breakTile({cell.x, cell.y + 1}))
        {
            return feet;
        }
        advanced_platformer::World world(advanced_platformer::composeItems(catalogs.items, 0));
        world.addPickup(pickup);
        const advanced_platformer::Aabb& bounds = world.pickups().front().body.bounds;
        const int steps = static_cast<int>(SettleSeconds / tests::FixedStepSeconds);
        for (int count = 0; count < steps; ++count)
        {
            const glm::vec2 before = advanced_platformer::feetOf(bounds);
            advanced_platformer::updatePickupMovement(broken, world, tests::FixedStepSeconds);
            if (advanced_platformer::feetOf(bounds) == before)
            {
                break;
            }
        }
        return advanced_platformer::feetOf(bounds);
    }

    bool playerReachesPickup(
        const GeneratedLevel& level,
        const advanced_platformer::PickupPlacement& placement,
        const advanced_platformer::GameCatalogs& catalogs)
    {
        const advanced_platformer::TileMap map =
            advanced_platformer::composeTileMap(level.mapRows, level.tileLegend, catalogs.tiles);
        advanced_platformer::Actor player = advanced_platformer::composePlayer(catalogs, 0);
        advanced_platformer::moveFeetTo(
            player.body.bounds, advanced_platformer::feetInCell(map.tileSize(), level.playerSpawn));
        const advanced_platformer::Pickup pickup = advanced_platformer::composePickup(
            advanced_platformer::pickupDefinition(catalogs.pickups, placement.definitionName),
            catalogs.items,
            0,
            advanced_platformer::feetInCell(map.tileSize(), placement.spawn));
        return advanced_platformer::actorCanReach(
                   map,
                   player,
                   advanced_platformer::feetOf(pickup.body.bounds),
                   tests::FixedStepSeconds) ||
               advanced_platformer::actorCanReach(
                   map,
                   player,
                   feetOnceTileBelowBreaks(map, pickup, catalogs),
                   tests::FixedStepSeconds);
    }
}

TEST_CASE(
    "Every shipped pickup is within the player's reach in each route level",
    "[app][content][generation]")
{
    const advanced_platformer::GameCatalogs catalogs =
        advanced_platformer::loadGameCatalogs("assets/catalogs");
    const RoomPieces shipped = shippedPieces();
    std::vector<Route> routes = doorPairRoutes(shipped);
    std::ranges::move(startRoutes(shipped), std::back_inserter(routes));
    std::ranges::move(exitRoutes(shipped), std::back_inserter(routes));
    for (const Route& route : routes)
    {
        const std::optional<GeneratedLevel> level = generateRoute(route);
        if (!level.has_value())
        {
            continue;
        }
        for (const advanced_platformer::PickupPlacement& pickup : level->pickups)
        {
            INFO(route.name << ": " << pickup.id);
            CHECK(playerReachesPickup(*level, pickup, catalogs));
        }
    }
}
