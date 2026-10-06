#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <vector>

#include "content/content_glaze.hpp"
#include "content/placements.hpp"
#include "content/room_pieces.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "support/json_document.hpp"

namespace
{
    using advanced_platformer::Cell;
    using advanced_platformer::RoomDoors;
    using advanced_platformer::RoomSide;
    constexpr const char* FixturePieces = "tests/fixtures/levels/rooms.json";
}

TEST_CASE(
    "A room piece file names its pieces, doors, roles and placements",
    "[app][content][generation]")
{
    const advanced_platformer::RoomPieceCatalog catalog =
        advanced_platformer::loadRoomPieceCatalog(FixturePieces);

    REQUIRE(catalog.roomSize.width == 8);
    REQUIRE(catalog.roomSize.height == 6);
    REQUIRE(catalog.wall == '#');
    REQUIRE(catalog.open == '.');
    REQUIRE(catalog.pieces.size() == 4);
    REQUIRE(catalog.pieces[0].playerSpawn == Cell{1, 4});
    const advanced_platformer::RoomPiece& hall = catalog.pieces[1];
    REQUIRE(hall.mirror);
    REQUIRE(hall.actors.size() == 1);
    REQUIRE(hall.actors[0].id == "test_guard_1");
    REQUIRE(hall.actors[0].definitionName == "test_guard");
    REQUIRE(hall.actors[0].spawn == Cell{6, 4});
    REQUIRE_FALSE(hall.actors[0].patrol.has_value());
    const advanced_platformer::RoomPiece& store = catalog.pieces[2];
    REQUIRE(store.name == "store");
    REQUIRE(store.role == advanced_platformer::RoomRole::Arena);
    REQUIRE(store.doors == advanced_platformer::withDoor({}, RoomSide::Left));
    REQUIRE_FALSE(store.mirror);
    REQUIRE(store.pickups.size() == 1);
    REQUIRE(store.pickups[0].id == "medicine_box_1");
    REQUIRE(store.pickups[0].definitionName == "medicine_box");
    REQUIRE(store.pickups[0].spawn == Cell{2, 4});
    const advanced_platformer::ExitPlacement exit =
        catalog.pieces[3].exit.value_or(advanced_platformer::ExitPlacement{});
    REQUIRE(exit.definitionName == "test_door");
    REQUIRE(exit.spawn == Cell{6, 4});
}

namespace
{
    tests::Json fixturePieces()
    {
        return tests::parseJson(advanced_platformer::loadContentText(FixturePieces));
    }

    void requireRejected(const tests::Json& document, const std::string& message)
    {
        REQUIRE_THROWS_WITH(
            advanced_platformer::parseRoomPieceCatalog(tests::dumpJson(document), "rooms.json"),
            Catch::Matchers::ContainsSubstring(message));
    }
}

TEST_CASE(
    "A room piece places actors with patrols and locks its exit",
    "[app][content][generation]")
{
    tests::Json document = fixturePieces();
    document["pieces"][1]["actors"][0]["patrol"] =
        tests::parseJson(R"({"first": [1, 4], "second": [6, 4]})");
    document["pieces"][3]["exit"] = tests::parseJson(
        R"({"definition": "test_door", "spawn": [6, 4], "requirement": {"item": "key", "quantity": 2}, "consumeItem": true})");

    const advanced_platformer::RoomPieceCatalog catalog =
        advanced_platformer::parseRoomPieceCatalog(tests::dumpJson(document), "rooms.json");

    const advanced_platformer::ActorPlacement& guard = catalog.pieces[1].actors[0];
    const advanced_platformer::PatrolPlacement patrol =
        guard.patrol.value_or(advanced_platformer::PatrolPlacement{});
    REQUIRE(patrol.first == Cell{1, 4});
    REQUIRE(patrol.second == Cell{6, 4});
    const advanced_platformer::ExitPlacement exit =
        catalog.pieces[3].exit.value_or(advanced_platformer::ExitPlacement{});
    REQUIRE(exit.requirement.has_value());
    REQUIRE(exit.requirement.value_or(advanced_platformer::NamedItemStack{}).item == "key");
    REQUIRE(exit.requirement.value_or(advanced_platformer::NamedItemStack{}).quantity == 2);
    REQUIRE(exit.consumeItem);
}

TEST_CASE("A room piece file rejects placements it cannot build", "[app][content][generation]")
{
    tests::Json document = fixturePieces();
    auto& pieces = document["pieces"];

    SECTION("A start room without a player spawn")
    {
        tests::eraseKey(pieces[0], "playerSpawn");
        requireRejected(document, "pieces[0].playerSpawn: start rooms need a player spawn");
    }
    SECTION("A player spawn outside a start room")
    {
        pieces[1]["playerSpawn"] = tests::numbers({1, 4});
        requireRejected(
            document, "pieces[1].playerSpawn: corridor rooms cannot have a player spawn");
    }
    SECTION("An exit room without an exit")
    {
        tests::eraseKey(pieces[3], "exit");
        requireRejected(document, "pieces[3].exit: exit rooms need an exit");
    }
    SECTION("An exit outside an exit room")
    {
        pieces[2]["exit"] = tests::parseJson(R"({"definition": "test_door", "spawn": [2, 4]})");
        requireRejected(document, "pieces[2].exit: arena rooms cannot have an exit");
    }
    SECTION("A spawn outside the piece")
    {
        pieces[1]["actors"][0]["spawn"] = tests::numbers({8, 4});
        requireRejected(
            document, "pieces[1].actors[0].spawn: expected a cell inside the 8 by 6 piece");
    }
    SECTION("A patrol point outside the piece")
    {
        pieces[1]["actors"][0]["patrol"] =
            tests::parseJson(R"({"first": [1, 4], "second": [1, -1]})");
        requireRejected(
            document, "pieces[1].actors[0].patrol.second: expected a cell inside the 8 by 6 piece");
    }
    SECTION("A spawn that is not a cell")
    {
        pieces[1]["actors"][0]["spawn"] = tests::numbers({6});
        requireRejected(document, "expected two whole numbers, [column, row]");
    }
    SECTION("An empty placement id")
    {
        pieces[1]["actors"][0]["id"] = "";
        requireRejected(document, "pieces[1].actors[0].id: placement id cannot be empty");
    }
    SECTION("A repeated placement id")
    {
        pieces[1]["pickups"] = tests::parseJson(
            R"([{"id": "test_guard_1", "definition": "medicine_box", "spawn": [2, 4]}])");
        requireRejected(
            document,
            "pieces[1].pickups[0].id: id 'test_guard_1' is already used by pieces[1].actors[0]");
    }
    SECTION("An empty definition")
    {
        pieces[2]["pickups"][0]["definition"] = "";
        requireRejected(
            document, "pieces[2].pickups[0].definition: pickup definition name cannot be empty");
    }
    SECTION("An exit with no definition")
    {
        pieces[3]["exit"]["definition"] = "";
        requireRejected(
            document, "pieces[3].exit.definition: exit definition name cannot be empty");
    }
    SECTION("An exit that asks for nothing of an item")
    {
        pieces[3]["exit"]["requirement"] = tests::parseJson(R"({"item": "key", "quantity": 0})");
        requireRejected(
            document, "pieces[3].exit.requirement.quantity: expected a positive integer, got 0");
    }
    SECTION("A placement field typo")
    {
        pieces[1]["actors"][0]["patroll"] = tests::emptyObject();
        requireRejected(document, "unknown field 'patroll'");
    }
}

TEST_CASE("A room piece file says how a run's levels grow", "[app][content][generation]")
{
    tests::Json document = fixturePieces();
    document["run"] =
        tests::parseJson(R"({"grid": [5, 3], "firstRooms": 4, "roomsPerLevel": 2, "maxRooms": 9})");

    const advanced_platformer::RoomPieceCatalog catalog =
        advanced_platformer::parseRoomPieceCatalog(tests::dumpJson(document), "rooms.json");

    REQUIRE(catalog.run.grid.width == 5);
    REQUIRE(catalog.run.grid.height == 3);
    REQUIRE(catalog.run.firstRooms == 4);
    REQUIRE(catalog.run.roomsPerLevel == 2);
    REQUIRE(catalog.run.maxRooms == 9);

    tests::eraseKey(document["run"], "grid");
    const advanced_platformer::RoomPieceCatalog defaulted =
        advanced_platformer::parseRoomPieceCatalog(tests::dumpJson(document), "rooms.json");
    REQUIRE(defaulted.run.grid.width == 9);
    REQUIRE(defaulted.run.grid.height == 7);
}

TEST_CASE("A room piece file rejects runs it cannot build", "[app][content][generation]")
{
    tests::Json document = fixturePieces();
    document["run"] =
        tests::parseJson(R"({"grid": [3, 3], "firstRooms": 4, "roomsPerLevel": 1, "maxRooms": 6})");
    auto& run = document["run"];

    SECTION("No run")
    {
        tests::eraseKey(document, "run");
        requireRejected(document, "rooms.json:");
    }
    SECTION("Too few first rooms")
    {
        run["firstRooms"] = 1;
        requireRejected(
            document, "run.firstRooms: expected at least 2 rooms and no more than maxRooms");
    }
    SECTION("More first rooms than the cap")
    {
        run["firstRooms"] = 7;
        requireRejected(
            document, "run.firstRooms: expected at least 2 rooms and no more than maxRooms");
    }
    SECTION("Fewer rooms each level")
    {
        run["roomsPerLevel"] = -1;
        requireRejected(document, "run.roomsPerLevel: expected zero or more rooms");
    }
    SECTION("A cap above what the grid holds")
    {
        run["maxRooms"] = 10;
        requireRejected(
            document,
            "run.maxRooms: expected at least 2 rooms and no more than the grid's 9 slots");
    }
    SECTION("An empty grid")
    {
        run["grid"] = tests::numbers({0, 3});
        requireRejected(document, "run.grid: expected a positive size");
    }
    SECTION("A field typo")
    {
        run["maxRoom"] = 6;
        requireRejected(document, "rooms.json:");
    }
}

TEST_CASE("A room piece file rejects pieces that cannot be stitched", "[app][content][generation]")
{
    tests::Json document = fixturePieces();
    auto& hall = document["pieces"][1];

    SECTION("A wrong number of rows")
    {
        hall["map"].get_array().pop_back();
        requireRejected(document, "rooms.json: pieces[1].map: expected 6 rows, got 5");
    }
    SECTION("A row of the wrong width")
    {
        hall["map"][1] = "#.....#";
        requireRejected(document, "pieces[1].map[1]: expected 8 columns, got 7");
    }
    SECTION("An unknown symbol")
    {
        hall["map"][1] = "#..?...#";
        requireRejected(document, "pieces[1].map[1][3]: unknown symbol '?'");
    }
    SECTION("A wall where a listed door should be open")
    {
        hall["map"][2] = "#.......";
        requireRejected(document, "pieces[1].map[2][0]: expected '.': an edge is open on a door");
    }
    SECTION("An opening in an edge away from the doors")
    {
        hall["map"][1] = ".......#";
        requireRejected(document, "pieces[1].map[1][0]: expected '#': an edge is wall");
    }
    SECTION("A repeated piece name")
    {
        hall["name"] = "start";
        requireRejected(document, "pieces[1].name: piece name 'start' is already used");
    }
    SECTION("A door listed twice")
    {
        hall["doors"] = tests::parseJson(R"(["left", "left"])");
        requireRejected(document, "pieces[1].doors: door left is listed twice");
    }
    SECTION("A wall symbol that is not a tile")
    {
        document["wall"] = "W";
        requireRejected(document, "wall: symbol is not in tileLegend");
    }
    SECTION("A legend without empty")
    {
        document["tileLegend"] = tests::parseJson(R"({".": "stone", "#": "stone"})");
        requireRejected(document, "tileLegend: expected a symbol for empty");
    }
    SECTION("An odd room width")
    {
        document["roomSize"] = tests::numbers({9, 6});
        requireRejected(document, "roomSize: expected an even width");
    }
}

TEST_CASE("Doors sit at the same cells on every room's edges", "[app][content][generation]")
{
    const advanced_platformer::GridSize size{8, 6};

    REQUIRE(
        advanced_platformer::doorCells(size, RoomSide::Left) ==
        std::vector<Cell>{{0, 2}, {0, 3}, {0, 4}});
    REQUIRE(
        advanced_platformer::doorCells(size, RoomSide::Right) ==
        std::vector<Cell>{{7, 2}, {7, 3}, {7, 4}});
    REQUIRE(
        advanced_platformer::doorCells(size, RoomSide::Up) ==
        std::vector<Cell>{{2, 0}, {3, 0}, {4, 0}, {5, 0}});
    REQUIRE(
        advanced_platformer::doorCells(size, RoomSide::Down) ==
        std::vector<Cell>{{2, 5}, {3, 5}, {4, 5}, {5, 5}});
}

TEST_CASE("Room doors are sets of sides that flip left to right", "[app][content][generation]")
{
    const RoomDoors leftAndUp = advanced_platformer::withDoor(
        advanced_platformer::withDoor({}, RoomSide::Left), RoomSide::Up);

    REQUIRE(advanced_platformer::hasDoor(leftAndUp, RoomSide::Left));
    REQUIRE_FALSE(advanced_platformer::hasDoor(leftAndUp, RoomSide::Right));
    REQUIRE(advanced_platformer::doorCount(leftAndUp) == 2);
    REQUIRE(
        advanced_platformer::mirroredDoors(leftAndUp) ==
        advanced_platformer::withDoor(
            advanced_platformer::withDoor({}, RoomSide::Right), RoomSide::Up));
    REQUIRE(
        advanced_platformer::coversDoors(
            leftAndUp, advanced_platformer::withDoor({}, RoomSide::Up)));
    REQUIRE_FALSE(
        advanced_platformer::coversDoors(
            leftAndUp, advanced_platformer::withDoor({}, RoomSide::Down)));
}
