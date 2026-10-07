#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <format>
#include <stdexcept>
#include <string>
#include <utility>
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
    constexpr const char* FixtureFolder = "tests/fixtures/rooms/rooms";
    constexpr const char* FixturePieces = "tests/fixtures/rooms/rooms/rooms.json";

    struct FixtureDocument
    {
        tests::Json catalog;
        std::vector<std::pair<std::string, tests::Json>> pieces;
    };

    FixtureDocument fixturePieces()
    {
        FixtureDocument document{
            tests::parseJson(advanced_platformer::loadContentText(FixturePieces)), {}};
        for (const char* name : {"exit", "hall", "start", "start_end", "store"})
        {
            document.pieces.emplace_back(
                name,
                tests::parseJson(
                    advanced_platformer::loadContentText(
                        std::format("{}/pieces/{}.json", FixtureFolder, name))));
        }
        return document;
    }

    tests::Json& piece(FixtureDocument& document, const std::string& name)
    {
        for (auto& [pieceName, json] : document.pieces)
        {
            if (pieceName == name)
            {
                return json;
            }
        }
        throw std::out_of_range(name);
    }

    advanced_platformer::RoomPieceCatalog parseFixture(const FixtureDocument& document)
    {
        std::vector<advanced_platformer::RoomPieceSource> pieces;
        pieces.reserve(document.pieces.size());
        for (const auto& [name, json] : document.pieces)
        {
            pieces.push_back({name, tests::dumpJson(json), std::format("pieces/{}.json", name)});
        }
        return advanced_platformer::parseRoomPieceCatalog(
            tests::dumpJson(document.catalog), "rooms.json", pieces);
    }

    void requireRejected(const FixtureDocument& document, const std::string& message)
    {
        REQUIRE_THROWS_WITH(parseFixture(document), Catch::Matchers::ContainsSubstring(message));
    }
}

TEST_CASE(
    "A room piece folder names its pieces by file, with doors, roles and placements",
    "[app][content][generation]")
{
    const advanced_platformer::RoomPieceCatalog catalog =
        advanced_platformer::loadRoomPieceCatalog(FixturePieces);

    REQUIRE(catalog.roomSize.width == 8);
    REQUIRE(catalog.roomSize.height == 6);
    REQUIRE(catalog.open == '.');
    REQUIRE(catalog.pieces.size() == 5);
    REQUIRE(catalog.pieces[2].name == "start");
    REQUIRE(catalog.pieces[2].playerSpawn == Cell{1, 4});
    REQUIRE(catalog.pieces[3].name == "start_end");
    const advanced_platformer::RoomPiece& hall = catalog.pieces[1];
    REQUIRE(hall.name == "hall");
    REQUIRE_FALSE(hall.mirror);
    REQUIRE(hall.actors.size() == 1);
    REQUIRE(hall.actors[0].id == "test_guard_1");
    REQUIRE(hall.actors[0].definitionName == "test_guard");
    REQUIRE(hall.actors[0].spawn == Cell{6, 4});
    REQUIRE_FALSE(hall.actors[0].patrol.has_value());
    const advanced_platformer::RoomPiece& store = catalog.pieces[4];
    REQUIRE(store.name == "store");
    REQUIRE(store.role == advanced_platformer::RoomRole::Arena);
    REQUIRE(store.doors == advanced_platformer::withDoor({}, RoomSide::Left));
    REQUIRE(store.mirror);
    REQUIRE(store.pickups.size() == 1);
    REQUIRE(store.pickups[0].id == "medicine_box_1");
    REQUIRE(store.pickups[0].definitionName == "medicine_box");
    REQUIRE(store.pickups[0].spawn == Cell{2, 4});
    const advanced_platformer::ExitPlacement exit =
        catalog.pieces[0].exit.value_or(advanced_platformer::ExitPlacement{});
    REQUIRE(catalog.pieces[0].name == "exit");
    REQUIRE(exit.definitionName == "test_door");
    REQUIRE(exit.spawn == Cell{6, 4});
}

TEST_CASE(
    "A room piece places actors with patrols and locks its exit",
    "[app][content][generation]")
{
    FixtureDocument document = fixturePieces();
    piece(document, "hall")["actors"][0]["patrol"] =
        tests::parseJson(R"({"first": [1, 4], "second": [6, 4]})");
    piece(document, "exit")["exit"] = tests::parseJson(
        R"({"definition": "test_door", "spawn": [6, 4], "requirement": {"item": "key", "quantity": 2}, "consumeItem": true})");

    const advanced_platformer::RoomPieceCatalog catalog = parseFixture(document);

    const advanced_platformer::ActorPlacement& guard = catalog.pieces[1].actors[0];
    const advanced_platformer::PatrolPlacement patrol =
        guard.patrol.value_or(advanced_platformer::PatrolPlacement{});
    REQUIRE(patrol.first == Cell{1, 4});
    REQUIRE(patrol.second == Cell{6, 4});
    const advanced_platformer::ExitPlacement exit =
        catalog.pieces[0].exit.value_or(advanced_platformer::ExitPlacement{});
    REQUIRE(exit.requirement.has_value());
    REQUIRE(exit.requirement.value_or(advanced_platformer::NamedItemStack{}).item == "key");
    REQUIRE(exit.requirement.value_or(advanced_platformer::NamedItemStack{}).quantity == 2);
    REQUIRE(exit.consumeItem);
}

TEST_CASE("A room piece folder rejects placements it cannot build", "[app][content][generation]")
{
    FixtureDocument document = fixturePieces();
    tests::Json& hall = piece(document, "hall");

    SECTION("A start room without a player spawn")
    {
        tests::eraseKey(piece(document, "start"), "playerSpawn");
        requireRejected(
            document, "pieces/start.json: playerSpawn: start rooms need a player spawn");
    }
    SECTION("A player spawn outside a start room")
    {
        hall["playerSpawn"] = tests::numbers({1, 4});
        requireRejected(
            document, "pieces/hall.json: playerSpawn: corridor rooms cannot have a player spawn");
    }
    SECTION("An exit room without an exit")
    {
        tests::eraseKey(piece(document, "exit"), "exit");
        requireRejected(document, "pieces/exit.json: exit: exit rooms need an exit");
    }
    SECTION("An exit outside an exit room")
    {
        piece(document, "store")["exit"] =
            tests::parseJson(R"({"definition": "test_door", "spawn": [2, 4]})");
        requireRejected(document, "pieces/store.json: exit: arena rooms cannot have an exit");
    }
    SECTION("A spawn outside the piece")
    {
        hall["actors"][0]["spawn"] = tests::numbers({8, 4});
        requireRejected(
            document, "pieces/hall.json: actors[0].spawn: expected a cell inside the 8 by 6 piece");
    }
    SECTION("A patrol point outside the piece")
    {
        hall["actors"][0]["patrol"] = tests::parseJson(R"({"first": [1, 4], "second": [1, -1]})");
        requireRejected(
            document,
            "pieces/hall.json: actors[0].patrol.second: expected a cell inside the 8 by 6 piece");
    }
    SECTION("A spawn that is not a cell")
    {
        hall["actors"][0]["spawn"] = tests::numbers({6});
        requireRejected(document, "expected two whole numbers, [column, row]");
    }
    SECTION("An empty placement id")
    {
        hall["actors"][0]["id"] = "";
        requireRejected(document, "pieces/hall.json: actors[0].id: placement id cannot be empty");
    }
    SECTION("A repeated placement id")
    {
        hall["pickups"] = tests::parseJson(
            R"([{"id": "test_guard_1", "definition": "medicine_box", "spawn": [2, 4]}])");
        requireRejected(
            document,
            "pieces/hall.json: pickups[0].id: id 'test_guard_1' is already used by actors[0]");
    }
    SECTION("An empty definition")
    {
        piece(document, "store")["pickups"][0]["definition"] = "";
        requireRejected(
            document,
            "pieces/store.json: pickups[0].definition: pickup definition name cannot be empty");
    }
    SECTION("An exit with no definition")
    {
        piece(document, "exit")["exit"]["definition"] = "";
        requireRejected(
            document, "pieces/exit.json: exit.definition: exit definition name cannot be empty");
    }
    SECTION("An exit that asks for nothing of an item")
    {
        piece(document, "exit")["exit"]["requirement"] =
            tests::parseJson(R"({"item": "key", "quantity": 0})");
        requireRejected(
            document,
            "pieces/exit.json: exit.requirement.quantity: expected a positive integer, got 0");
    }
    SECTION("A placement field typo")
    {
        hall["actors"][0]["patroll"] = tests::emptyObject();
        requireRejected(document, "unknown field 'patroll'");
    }
}

TEST_CASE("A room piece file says how a run's levels grow", "[app][content][generation]")
{
    FixtureDocument document = fixturePieces();
    document.catalog["run"] =
        tests::parseJson(R"({"grid": [5, 3], "firstRooms": 4, "roomsPerLevel": 2, "maxRooms": 9})");

    const advanced_platformer::RoomPieceCatalog catalog = parseFixture(document);

    REQUIRE(catalog.run.grid.width == 5);
    REQUIRE(catalog.run.grid.height == 3);
    REQUIRE(catalog.run.firstRooms == 4);
    REQUIRE(catalog.run.roomsPerLevel == 2);
    REQUIRE(catalog.run.maxRooms == 9);

    tests::eraseKey(document.catalog["run"], "grid");
    const advanced_platformer::RoomPieceCatalog defaulted = parseFixture(document);
    REQUIRE(defaulted.run.grid.width == 9);
    REQUIRE(defaulted.run.grid.height == 7);
}

TEST_CASE("A room piece file rejects runs it cannot build", "[app][content][generation]")
{
    FixtureDocument document = fixturePieces();
    document.catalog["run"] =
        tests::parseJson(R"({"grid": [3, 3], "firstRooms": 4, "roomsPerLevel": 1, "maxRooms": 6})");
    auto& run = document.catalog["run"];

    SECTION("No run")
    {
        tests::eraseKey(document.catalog, "run");
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

TEST_CASE(
    "A room piece folder rejects pieces that cannot be stitched",
    "[app][content][generation]")
{
    FixtureDocument document = fixturePieces();
    tests::Json& hall = piece(document, "hall");

    SECTION("A wrong number of rows")
    {
        hall["map"].get_array().pop_back();
        requireRejected(document, "pieces/hall.json: map: expected 6 rows, got 5");
    }
    SECTION("A row of the wrong width")
    {
        hall["map"][1] = "#.....#";
        requireRejected(document, "pieces/hall.json: map[1]: expected 8 columns, got 7");
    }
    SECTION("An unknown symbol")
    {
        hall["map"][1] = "#..?...#";
        requireRejected(document, "pieces/hall.json: map[1][3]: unknown symbol '?'");
    }
    SECTION("A wall where a listed door should be open")
    {
        hall["map"][2] = "#.......";
        requireRejected(
            document, "pieces/hall.json: map[2][0]: expected '.': an edge is open on a door");
    }
    SECTION("An opening in an edge away from the doors")
    {
        hall["map"][1] = ".......#";
        requireRejected(
            document,
            "pieces/hall.json: map[1][0]: expected a tile, not '.': an edge is solid away from the "
            "doors");
    }
    SECTION("A door listed twice")
    {
        hall["doors"] = tests::parseJson(R"(["left", "left"])");
        requireRejected(document, "pieces/hall.json: doors: door left is listed twice");
    }
    SECTION("A piece field typo")
    {
        hall["name"] = "hall";
        requireRejected(document, "unknown field 'name'");
    }
    SECTION("A legend without empty")
    {
        document.catalog["tileLegend"] = tests::parseJson(R"({".": "stone", "#": "stone"})");
        requireRejected(document, "tileLegend: expected a symbol for empty");
    }
    SECTION("An odd room width")
    {
        document.catalog["roomSize"] = tests::numbers({9, 6});
        requireRejected(document, "roomSize: expected an even width");
    }
    SECTION("A folder without pieces")
    {
        REQUIRE_THROWS_WITH(
            advanced_platformer::loadRoomPieceCatalog("tests/fixtures/rooms/rooms.json"),
            Catch::Matchers::ContainsSubstring(
                "tests/fixtures/rooms/rooms.json: expected piece files in "
                "tests/fixtures/rooms/pieces"));
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
}
