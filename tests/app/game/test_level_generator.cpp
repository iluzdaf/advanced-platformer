#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstddef>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "game/level_generator.hpp"
#include "content/placements.hpp"
#include "content/room_pieces.hpp"
#include "advanced_platformer/math/coordinates.hpp"

namespace
{
    using advanced_platformer::Cell;
    using advanced_platformer::GridSize;
    using advanced_platformer::LevelRandom;
    using advanced_platformer::RoomLayout;
    using advanced_platformer::RoomSide;
    using advanced_platformer::RoomSlot;

    constexpr const char* FixturePieces = "tests/fixtures/levels/rooms.json";

    advanced_platformer::LevelGeneration generation(std::uint32_t seed)
    {
        return {.grid = {5, 5}, .roomCount = 6, .seed = seed};
    }
}

namespace
{
    const RoomSlot* roomAt(const RoomLayout& layout, Cell grid)
    {
        for (const RoomSlot& room : layout.rooms)
        {
            if (room.grid == grid)
            {
                return &room;
            }
        }
        return nullptr;
    }
}

TEST_CASE("A room layout is a tree grown from the grid's centre", "[app][content][generation]")
{
    const GridSize grid{9, 7};
    for (std::uint64_t seed = 1; seed <= 50; ++seed)
    {
        LevelRandom random{seed};
        const RoomLayout layout = advanced_platformer::layoutRooms(grid, 12, random);
        INFO("seed " << seed);

        REQUIRE(layout.rooms.size() == 12);
        REQUIRE(layout.rooms.front().grid == Cell{4, 3});
        REQUIRE(layout.rooms.front().depth == 0);
        std::set<std::pair<int, int>> slots;
        int doors = 0;
        int deepest = 0;
        for (const RoomSlot& room : layout.rooms)
        {
            REQUIRE(room.grid.x >= 0);
            REQUIRE(room.grid.x < grid.width);
            REQUIRE(room.grid.y >= 0);
            REQUIRE(room.grid.y < grid.height);
            REQUIRE(slots.insert({room.grid.x, room.grid.y}).second);
            deepest = std::max(deepest, room.depth);
            for (const RoomSide side :
                 {RoomSide::Left, RoomSide::Right, RoomSide::Up, RoomSide::Down})
            {
                const RoomSlot* neighbour =
                    roomAt(layout, advanced_platformer::stepTowards(room.grid, side));
                const bool joined = advanced_platformer::hasDoor(room.doors, side);
                REQUIRE(joined == (neighbour != nullptr));
                if (joined)
                {
                    REQUIRE(
                        advanced_platformer::hasDoor(
                            neighbour->doors, advanced_platformer::oppositeOf(side)));
                    REQUIRE(std::abs(neighbour->depth - room.depth) == 1);
                    ++doors;
                }
            }
        }
        REQUIRE(doors == 2 * 11);
        const RoomSlot& exit = layout.rooms[layout.exit];
        REQUIRE(exit.depth == deepest);
        REQUIRE(advanced_platformer::doorCount(exit.doors) == 1);
    }
}

TEST_CASE("The same seed lays out the same rooms", "[app][content][generation]")
{
    LevelRandom first{7};
    LevelRandom second{7};
    const RoomLayout one = advanced_platformer::layoutRooms({9, 7}, 10, first);
    const RoomLayout two = advanced_platformer::layoutRooms({9, 7}, 10, second);

    REQUIRE(one.exit == two.exit);
    REQUIRE(one.rooms.size() == two.rooms.size());
    for (std::size_t room = 0; room < one.rooms.size(); ++room)
    {
        REQUIRE(one.rooms[room].grid == two.rooms[room].grid);
        REQUIRE(one.rooms[room].doors == two.rooms[room].doors);
    }
}

TEST_CASE("Stitched rooms share walls and seal unused doors", "[app][content][generation]")
{
    const advanced_platformer::RoomPieceCatalog catalog =
        advanced_platformer::loadRoomPieceCatalog(FixturePieces);
    const RoomLayout layout{
        .grid = {3, 1},
        .rooms =
            {{{0, 0}, advanced_platformer::withDoor({}, RoomSide::Right), 0},
             {{1, 0},
              advanced_platformer::withDoor(
                  advanced_platformer::withDoor({}, RoomSide::Left), RoomSide::Right),
              1},
             {{2, 0}, advanced_platformer::withDoor({}, RoomSide::Left), 2}},
        .exit = 2};
    const advanced_platformer::LevelData level =
        advanced_platformer::stitchRooms(catalog, layout, {{0, false}, {1, false}, {3, false}});

    REQUIRE(level.mapRows.size() == 6);
    REQUIRE(level.mapRows.front().size() == 3 * 7 + 1);
    for (int row = 2; row <= 4; ++row)
    {
        const std::string& line = level.mapRows[static_cast<std::size_t>(row)];
        REQUIRE(line[0] == '#');
        REQUIRE(line[7] == '.');
        REQUIRE(line[14] == '.');
        REQUIRE(line[21] == '#');
    }
    REQUIRE(level.mapRows.front() == std::string(22, '#'));
    REQUIRE(level.mapRows.back() == std::string(22, '#'));
    REQUIRE(level.playerSpawn == Cell{1, 4});
    REQUIRE(level.exit.definitionName == "test_door");
    REQUIRE(level.exit.spawn == Cell{14 + 6, 4});
    REQUIRE(level.actors.size() == 1);
    REQUIRE(level.actors.front().id == "room1_test_guard_1");
    REQUIRE(level.actors.front().definitionName == "test_guard");
    REQUIRE(level.actors.front().spawn == Cell{7 + 6, 4});
}

TEST_CASE("A mirrored piece flips its placements with its map", "[app][content][generation]")
{
    advanced_platformer::RoomPieceCatalog catalog =
        advanced_platformer::loadRoomPieceCatalog(FixturePieces);
    catalog.pieces[1].actors[0].patrol = advanced_platformer::PatrolPlacement{{1, 4}, {6, 4}};
    catalog.pieces[1].pickups.push_back(
        {.id = "box", .definitionName = "medicine_box", .spawn = {2, 3}});
    const RoomLayout layout{
        .grid = {2, 1},
        .rooms =
            {{{0, 0}, advanced_platformer::withDoor({}, RoomSide::Right), 0},
             {{1, 0}, advanced_platformer::withDoor({}, RoomSide::Left), 1}},
        .exit = 1};
    const advanced_platformer::LevelData level =
        advanced_platformer::stitchRooms(catalog, layout, {{1, true}, {3, false}});

    REQUIRE(level.actors.front().spawn == Cell{1, 4});
    const advanced_platformer::PatrolPlacement patrol =
        level.actors.front().patrol.value_or(advanced_platformer::PatrolPlacement{});
    REQUIRE(patrol.first == Cell{6, 4});
    REQUIRE(patrol.second == Cell{1, 4});
    REQUIRE(level.pickups.front().id == "room0_box");
    REQUIRE(level.pickups.front().spawn == Cell{5, 3});
    REQUIRE(level.exit.spawn == Cell{7 + 6, 4});
}

namespace
{
    std::string placementText(const std::string& id, Cell cell)
    {
        return std::format("{}@{},{};", id, cell.x, cell.y);
    }

    std::string levelText(const advanced_platformer::LevelData& level)
    {
        std::string text;
        for (const std::string& row : level.mapRows)
        {
            text += row + '\n';
        }
        text += placementText("player", level.playerSpawn);
        text += placementText("exit", level.exit.spawn);
        for (const advanced_platformer::ActorPlacement& actor : level.actors)
        {
            text += placementText(actor.id, actor.spawn);
        }
        for (const advanced_platformer::PickupPlacement& pickup : level.pickups)
        {
            text += placementText(pickup.id, pickup.spawn);
        }
        return text;
    }
}

TEST_CASE("The same seed generates the same level", "[app][content][generation]")
{
    const advanced_platformer::RoomPieceCatalog catalog =
        advanced_platformer::loadRoomPieceCatalog(FixturePieces);
    const std::string first =
        levelText(advanced_platformer::generateLevel(catalog, generation(7), "rooms.json"));

    REQUIRE(
        levelText(advanced_platformer::generateLevel(catalog, generation(7), "rooms.json")) ==
        first);
    std::set<std::string> levels;
    for (std::uint32_t seed = 1; seed <= 10; ++seed)
    {
        levels.insert(
            levelText(advanced_platformer::generateLevel(catalog, generation(seed), "rooms.json")));
    }
    REQUIRE(levels.size() > 1);
}

TEST_CASE(
    "A level fails to generate when no piece fits a room's doors",
    "[app][content][generation]")
{
    advanced_platformer::RoomPieceCatalog catalog =
        advanced_platformer::loadRoomPieceCatalog(FixturePieces);
    std::erase_if(
        catalog.pieces,
        [](const advanced_platformer::RoomPiece& piece)
        {
            return piece.role != advanced_platformer::RoomRole::Start &&
                   piece.role != advanced_platformer::RoomRole::Exit;
        });

    REQUIRE_THROWS_WITH(
        advanced_platformer::generateLevel(catalog, generation(7), "rooms.json"),
        Catch::Matchers::StartsWith("rooms.json: no corridor, shaft or arena piece has doors "));
}

TEST_CASE(
    "The level random numbers repeat for a seed and stay in range",
    "[app][content][generation]")
{
    LevelRandom first{42};
    LevelRandom second{42};
    for (int draw = 0; draw < 100; ++draw)
    {
        REQUIRE(advanced_platformer::nextRandom(first) == advanced_platformer::nextRandom(second));
        REQUIRE(advanced_platformer::randomBelow(first, 7) < 7);
        advanced_platformer::randomBelow(second, 7);
    }
}

TEST_CASE("Each level adds rooms until the run's cap", "[app][content][generation]")
{
    const advanced_platformer::RunSettings run{
        .grid = {9, 7}, .firstRooms = 4, .roomsPerLevel = 2, .maxRooms = 9};

    REQUIRE(advanced_platformer::roomsForLevel(run, 1) == 4);
    REQUIRE(advanced_platformer::roomsForLevel(run, 2) == 6);
    REQUIRE(advanced_platformer::roomsForLevel(run, 3) == 8);
    REQUIRE(advanced_platformer::roomsForLevel(run, 4) == 9);
    REQUIRE(advanced_platformer::roomsForLevel(run, 1000000) == 9);
    REQUIRE_THROWS_AS(advanced_platformer::roomsForLevel(run, 0), std::invalid_argument);
    const advanced_platformer::LevelGeneration generation =
        advanced_platformer::levelGeneration(run, 2, 17);
    REQUIRE(generation.roomCount == 6);
    REQUIRE(generation.seed == 17);
    REQUIRE(generation.grid.width == 9);
    REQUIRE(generation.grid.height == 7);
}

TEST_CASE("Level seeds follow from the run seed", "[app][content][generation]")
{
    REQUIRE(advanced_platformer::runLevelSeed(5, 1) == advanced_platformer::runLevelSeed(5, 1));
    REQUIRE(advanced_platformer::runLevelSeed(5, 1) != advanced_platformer::runLevelSeed(5, 2));
    REQUIRE(advanced_platformer::runLevelSeed(5, 1) != advanced_platformer::runLevelSeed(6, 1));
    REQUIRE(advanced_platformer::nextRunSeed(5) == advanced_platformer::nextRunSeed(5));
    REQUIRE(advanced_platformer::nextRunSeed(5) != 5);
}
