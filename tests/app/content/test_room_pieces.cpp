#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <vector>

#include "content/content_glaze.hpp"
#include "content/room_pieces.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "support/json_document.hpp"

namespace
{
    using advanced_platformer::Cell;
    using advanced_platformer::RoomDoors;
    using advanced_platformer::RoomSide;

    constexpr const char* FixturePieces = "tests/fixtures/levels/rooms.json";

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

TEST_CASE(
    "A room piece file names its pieces, doors, roles and markers",
    "[app][content][generation]")
{
    const advanced_platformer::RoomPieceCatalog catalog =
        advanced_platformer::loadRoomPieceCatalog(FixturePieces);

    REQUIRE(catalog.roomSize.width == 8);
    REQUIRE(catalog.roomSize.height == 6);
    REQUIRE(catalog.wall == '#');
    REQUIRE(catalog.open == '.');
    REQUIRE(catalog.startMarker == 'S');
    REQUIRE(catalog.exitMarker == 'E');
    REQUIRE(catalog.actorMarkers.at('g') == "test_guard");
    REQUIRE(catalog.pickupMarkers.at('m') == "medicine_box");
    REQUIRE(catalog.exitDefinition == "test_door");
    REQUIRE(catalog.pieces.size() == 4);
    const advanced_platformer::RoomPiece& store = catalog.pieces[2];
    REQUIRE(store.name == "store");
    REQUIRE(store.role == advanced_platformer::RoomRole::Arena);
    REQUIRE(store.doors == advanced_platformer::withDoor({}, RoomSide::Left));
    REQUIRE_FALSE(store.mirror);
    REQUIRE(catalog.pieces[1].mirror);
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
    SECTION("A start marker outside a start room")
    {
        hall["map"][1] = "#..S...#";
        requireRejected(document, "a corridor room has 0 start and 0 exit markers, not 1 and 0");
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
    SECTION("A marker that is also a tile")
    {
        document["markers"]["actors"] = tests::parseJson(R"({"#": "test_guard"})");
        requireRejected(document, "markers.actors.#: symbol is already a tile or marker");
    }
    SECTION("No ordinary room for some doors")
    {
        hall["doors"] = tests::parseJson(R"(["left", "right", "up"])");
        hall["map"][5] = "########";
        requireRejected(document, "no corridor, shaft or arena has doors");
    }
    SECTION("An odd room width")
    {
        document["roomSize"] = tests::numbers({9, 6});
        requireRejected(document, "roomSize: expected an even width");
    }
}
