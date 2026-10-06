#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "content/game_catalogs.hpp"
#include "content/run_settings.hpp"
#include "content/level_data.hpp"
#include "content/level_generator.hpp"
#include "content/room_pieces.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "support/atlas_size.hpp"
#include "support/player_reach.hpp"
#include "support/run_levels.hpp"

namespace
{
    using advanced_platformer::Cell;
    using advanced_platformer::RoomChoice;
    using advanced_platformer::RoomDoors;
    using advanced_platformer::RoomLayout;
    using advanced_platformer::RoomPieceCatalog;
    using advanced_platformer::RoomRole;
    using advanced_platformer::RoomSide;
    using advanced_platformer::RoomSlot;
    constexpr std::array AllSides{RoomSide::Left, RoomSide::Right, RoomSide::Up, RoomSide::Down};
    constexpr Cell Centre{1, 1};

    RoomDoors doorsOf(const RoomPieceCatalog& catalog, RoomChoice choice)
    {
        const RoomDoors doors = catalog.pieces[choice.piece].doors;
        return choice.mirrored ? mirroredDoors(doors) : doors;
    }

    std::vector<RoomChoice> orientations(const RoomPieceCatalog& catalog, RoomRole role)
    {
        std::vector<RoomChoice> result;
        for (std::size_t piece = 0; piece < catalog.pieces.size(); ++piece)
        {
            if (catalog.pieces[piece].role != role)
            {
                continue;
            }
            result.push_back({piece, false});
            if (catalog.pieces[piece].mirror)
            {
                result.push_back({piece, true});
            }
        }
        return result;
    }

    RoomChoice firstWithDoor(const RoomPieceCatalog& catalog, RoomRole role, RoomSide side)
    {
        for (const RoomChoice choice : orientations(catalog, role))
        {
            if (hasDoor(doorsOf(catalog, choice), side))
            {
                return choice;
            }
        }
        FAIL("No piece of the role has the door");
        return {};
    }

    struct Route
    {
        std::vector<RoomSlot> rooms;
        std::vector<RoomChoice> choices;
    };

    bool routeWorks(const RoomPieceCatalog& catalog, const Route& route)
    {
        static const advanced_platformer::GameCatalogs shippedCatalogs =
            advanced_platformer::loadGameCatalogs("assets/catalogs", tests::ShippedAtlasSize);
        const RoomLayout layout{
            .grid = {3, 3}, .rooms = route.rooms, .exit = route.rooms.size() - 1};
        const advanced_platformer::LevelData level =
            advanced_platformer::stitchRooms(catalog, layout, route.choices, "rooms.json");
        return tests::playerReachesExit(level, shippedCatalogs);
    }

    std::string describe(const RoomPieceCatalog& catalog, RoomChoice choice)
    {
        return catalog.pieces[choice.piece].name + (choice.mirrored ? " (mirrored)" : "");
    }
}

TEST_CASE("Every shipped run level reaches its exit across seeds", "[app][content][generation]")
{
    const advanced_platformer::RunSettings run =
        advanced_platformer::loadRunSettings("assets/levels/run.json");
    const advanced_platformer::GameCatalogs catalogs =
        advanced_platformer::loadGameCatalogs("assets/catalogs", tests::ShippedAtlasSize);
    const advanced_platformer::RoomPieceCatalog pieces =
        advanced_platformer::loadRoomPieceCatalog(run.levelDirectory / run.relativePieces);
    for (int number = 1; number <= tests::levelsUntilCap(run); ++number)
    {
        for (std::uint32_t seed = 1; seed <= 20; ++seed)
        {
            INFO("level " << number << " seed " << seed);
            REQUIRE(
                tests::playerReachesExit(
                    advanced_platformer::generateLevel(
                        pieces,
                        advanced_platformer::levelGeneration(run, number, seed),
                        "rooms.json"),
                    catalogs));
        }
    }
}

TEST_CASE("Every shipped room piece joins each pair of its doors", "[app][content][generation]")
{
    const RoomPieceCatalog catalog =
        advanced_platformer::loadRoomPieceCatalog("assets/levels/rooms.json");
    for (const RoomRole role : {RoomRole::Corridor, RoomRole::Shaft, RoomRole::Arena})
    {
        for (const RoomChoice piece : orientations(catalog, role))
        {
            const RoomDoors doors = doorsOf(catalog, piece);
            for (const RoomSide from : AllSides)
            {
                for (const RoomSide to : AllSides)
                {
                    if (from == to || !hasDoor(doors, from) || !hasDoor(doors, to))
                    {
                        continue;
                    }
                    const RoomSide startDoor = advanced_platformer::oppositeOf(from);
                    const RoomSide exitDoor = advanced_platformer::oppositeOf(to);
                    const Route route{
                        {{advanced_platformer::stepTowards(Centre, from),
                          withDoor(RoomDoors{}, startDoor),
                          0},
                         {Centre, withDoor(withDoor(RoomDoors{}, from), to), 1},
                         {advanced_platformer::stepTowards(Centre, to),
                          withDoor(RoomDoors{}, exitDoor),
                          2}},
                        {firstWithDoor(catalog, RoomRole::Start, startDoor),
                         piece,
                         firstWithDoor(catalog, RoomRole::Exit, exitDoor)}};
                    INFO(
                        describe(catalog, piece) << " from " << advanced_platformer::nameOf(from)
                                                 << " to " << advanced_platformer::nameOf(to));
                    CHECK(routeWorks(catalog, route));
                }
            }
        }
    }
}

TEST_CASE(
    "Every shipped start room reaches an exit through each of its doors",
    "[app][content][generation]")
{
    const RoomPieceCatalog catalog =
        advanced_platformer::loadRoomPieceCatalog("assets/levels/rooms.json");
    for (const RoomRole role : {RoomRole::Start, RoomRole::Exit})
    {
        for (const RoomChoice piece : orientations(catalog, role))
        {
            for (const RoomSide side : AllSides)
            {
                if (!hasDoor(doorsOf(catalog, piece), side))
                {
                    continue;
                }
                const RoomSide other = advanced_platformer::oppositeOf(side);
                const RoomSlot here{Centre, withDoor(RoomDoors{}, side), 0};
                const RoomSlot there{
                    advanced_platformer::stepTowards(Centre, side),
                    withDoor(RoomDoors{}, other),
                    1};
                Route route;
                if (role == RoomRole::Start)
                {
                    route = {{here, there}, {piece, firstWithDoor(catalog, RoomRole::Exit, other)}};
                }
                else
                {
                    route = {
                        {{there.grid, there.doors, 0}, {here.grid, here.doors, 1}},
                        {firstWithDoor(catalog, RoomRole::Start, other), piece}};
                }
                INFO(describe(catalog, piece) << " through " << advanced_platformer::nameOf(side));
                CHECK(routeWorks(catalog, route));
            }
        }
    }
}
