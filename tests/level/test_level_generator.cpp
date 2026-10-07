#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "advanced_platformer/level/level_generator.hpp"
#include "advanced_platformer/level/placements.hpp"
#include "advanced_platformer/level/room_pieces.hpp"
#include "content/room_pieces.hpp"
#include "advanced_platformer/math/coordinates.hpp"

namespace
{
    using advanced_platformer::Cell;
    using advanced_platformer::GeneratedLevel;
    using advanced_platformer::RoomPieces;

    constexpr const char* FixturePieces = "tests/fixtures/rooms/rooms/pieces.json";

    RoomPieces fixtureCatalog(advanced_platformer::GridSize grid, int rooms)
    {
        RoomPieces catalog = advanced_platformer::loadRoomPieceCatalog(FixturePieces);
        catalog.run = {.grid = grid, .firstRooms = rooms, .roomsPerLevel = 0, .maxRooms = rooms};
        return catalog;
    }
}

namespace
{
    std::string placementText(const std::string& id, Cell cell)
    {
        return std::format("{}@{},{};", id, cell.x, cell.y);
    }

    std::string levelText(const GeneratedLevel& level)
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
    const RoomPieces catalog = fixtureCatalog({5, 1}, 5);
    const std::string first =
        levelText(advanced_platformer::generateLevel(catalog, 1, 7, "pieces.json"));

    REQUIRE(levelText(advanced_platformer::generateLevel(catalog, 1, 7, "pieces.json")) == first);
    std::set<std::string> levels;
    for (std::uint32_t seed = 1; seed <= 10; ++seed)
    {
        levels.insert(
            levelText(advanced_platformer::generateLevel(catalog, 1, seed, "pieces.json")));
    }
    REQUIRE(levels.size() > 1);
}

TEST_CASE("Stitched rooms share the wall between them", "[app][content][generation]")
{
    const RoomPieces catalog = fixtureCatalog({5, 1}, 5);
    for (std::uint32_t seed = 1; seed <= 10; ++seed)
    {
        const GeneratedLevel level =
            advanced_platformer::generateLevel(catalog, 1, seed, "pieces.json");
        INFO("seed " << seed);

        REQUIRE(level.mapRows.size() == 6);
        REQUIRE(level.mapRows.front().size() == 5 * 7 + 1);
        REQUIRE(level.mapRows.front() == std::string(36, '#'));
        REQUIRE(level.mapRows.back() == std::string(36, '#'));
        for (int row = 2; row <= 4; ++row)
        {
            const std::string& line = level.mapRows[static_cast<std::size_t>(row)];
            REQUIRE(line[0] == '#');
            REQUIRE(line[7] == '.');
            REQUIRE(line[14] == '.');
            REQUIRE(line[21] == '.');
            REQUIRE(line[28] == '.');
            REQUIRE(line[35] == '#');
        }
        REQUIRE(level.playerSpawn.y == 4);
        REQUIRE((level.playerSpawn.x == 15 || level.playerSpawn.x == 20));
        REQUIRE(level.exit.definitionName == "test_door");
        REQUIRE(level.actors.size() == 2);
        std::vector<Cell> guards;
        for (const advanced_platformer::ActorPlacement& actor : level.actors)
        {
            REQUIRE(actor.definitionName == "test_guard");
            REQUIRE(actor.id.ends_with("_test_guard_1"));
            guards.push_back(actor.spawn);
        }
        REQUIRE(std::ranges::count(guards, Cell{13, 4}) == 1);
        REQUIRE(std::ranges::count(guards, Cell{27, 4}) == 1);
        REQUIRE(level.pickups.size() == 1);
        REQUIRE(level.pickups.front().id.ends_with("_medicine_box_1"));
        if (level.exit.spawn == Cell{34, 4})
        {
            REQUIRE(level.pickups.front().spawn == Cell{5, 4});
        }
        else
        {
            REQUIRE(level.exit.spawn == Cell{1, 4});
            REQUIRE(level.pickups.front().spawn == Cell{30, 4});
        }
    }
}

TEST_CASE("A mirrored piece flips its placements with its map", "[app][content][generation]")
{
    RoomPieces catalog = fixtureCatalog({2, 1}, 2);
    catalog.pieces[3].actors.push_back(
        {.id = "guard",
         .definitionName = "test_guard",
         .spawn = {1, 4},
         .patrol = advanced_platformer::PatrolPlacement{{1, 4}, {6, 4}}});
    const GeneratedLevel level = advanced_platformer::generateLevel(catalog, 1, 1, "pieces.json");

    REQUIRE(level.mapRows[3] == "#.............#");
    REQUIRE(level.playerSpawn == Cell{13, 4});
    REQUIRE(level.actors.size() == 1);
    REQUIRE(level.actors.front().id == "room0_guard");
    REQUIRE(level.actors.front().spawn == Cell{13, 4});
    const advanced_platformer::PatrolPlacement patrol =
        level.actors.front().patrol.value_or(advanced_platformer::PatrolPlacement{});
    REQUIRE(patrol.first == Cell{13, 4});
    REQUIRE(patrol.second == Cell{8, 4});
    REQUIRE(level.exit.spawn == Cell{1, 4});
}

TEST_CASE(
    "A level fails to generate when no piece fits a room's doors",
    "[app][content][generation]")
{
    RoomPieces catalog = fixtureCatalog({5, 1}, 5);
    std::erase_if(
        catalog.pieces,
        [](const advanced_platformer::RoomPiece& piece)
        {
            return piece.role != advanced_platformer::RoomRole::Start &&
                   piece.role != advanced_platformer::RoomRole::Exit;
        });

    REQUIRE_THROWS_WITH(
        advanced_platformer::generateLevel(catalog, 1, 7, "pieces.json"),
        Catch::Matchers::StartsWith("pieces.json: no corridor, shaft or arena piece has doors "));
}

TEST_CASE("Each level adds rooms until the run's cap", "[app][content][generation]")
{
    RoomPieces catalog = advanced_platformer::loadRoomPieceCatalog(FixturePieces);
    catalog.run = {.grid = {9, 1}, .firstRooms = 3, .roomsPerLevel = 2, .maxRooms = 7};
    const auto roomsIn = [&](int levelNumber)
    {
        const GeneratedLevel level =
            advanced_platformer::generateLevel(catalog, levelNumber, 17, "pieces.json");
        return (static_cast<int>(level.mapRows.front().size()) - 1) / 7;
    };

    REQUIRE(roomsIn(1) == 3);
    REQUIRE(roomsIn(2) == 5);
    REQUIRE(roomsIn(3) == 7);
    REQUIRE(roomsIn(4) == 7);
    REQUIRE(roomsIn(1000000) == 7);
    REQUIRE_THROWS_AS(
        advanced_platformer::generateLevel(catalog, 0, 17, "pieces.json"), std::invalid_argument);
}

TEST_CASE("Level seeds follow from the run seed", "[app][content][generation]")
{
    REQUIRE(advanced_platformer::runLevelSeed(5, 1) == advanced_platformer::runLevelSeed(5, 1));
    REQUIRE(advanced_platformer::runLevelSeed(5, 1) != advanced_platformer::runLevelSeed(5, 2));
    REQUIRE(advanced_platformer::runLevelSeed(5, 1) != advanced_platformer::runLevelSeed(6, 1));
    REQUIRE(advanced_platformer::nextRunSeed(5) == advanced_platformer::nextRunSeed(5));
    REQUIRE(advanced_platformer::nextRunSeed(5) != 5);
}
