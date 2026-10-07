#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <stdexcept>

#include "content/game_catalogs.hpp"
#include "content/item_catalog.hpp"
#include "content/room_pieces.hpp"
#include "game/level_composition.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "support/atlas_size.hpp"

TEST_CASE("Pickups and exit requirements resolve through level composition", "[app][pickups]")
{
    const auto pieces = advanced_platformer::loadRoomPieceCatalog(
        "tests/fixtures/rooms/pickup_placement/pieces.json");
    const auto gameCatalogs =
        advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize);
    const auto gameLevel = advanced_platformer::composeLevel(
        pieces, 1, 1, 7, gameCatalogs, advanced_platformer::composePlayer(gameCatalogs, 7));
    const auto& itemCatalog = gameCatalogs.items;
    REQUIRE(gameLevel.world.pickups().size() == 2);
    REQUIRE(
        gameLevel.world.pickups()[0].stack.item ==
        advanced_platformer::itemDefinition(itemCatalog, "medicine").id);
    REQUIRE(
        gameLevel.world.pickups()[1].stack.item ==
        advanced_platformer::itemDefinition(itemCatalog, "key").id);
    REQUIRE(gameLevel.world.pickups()[1].body.bounds.size == glm::vec2{10, 12});
    const auto& exit = gameLevel.world.exit();
    if (!exit || !exit->requirement)
    {
        throw std::logic_error("Missing fixture exit requirement");
    }
    REQUIRE(exit->requirement->item == advanced_platformer::itemDefinition(itemCatalog, "key").id);
}

TEST_CASE("Level composition reuses the supplied session item catalog", "[app][pickups]")
{
    const auto itemCatalog = advanced_platformer::parseItemCatalog(
        R"({"items":{
        "aaa":{"name":"Extra item","icon":{"position":[0,0],"size":[8,8]},"maximumStack":1},
        "key":{"name":"Session key","icon":{"position":[8,0],"size":[8,8]},"maximumStack":2},
        "medicine":{"name":"Medicine","icon":{"position":[16,0],"size":[8,8]},"maximumStack":3}
    }})",
        "session items");
    auto gameCatalogs =
        advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize);
    gameCatalogs.items = itemCatalog;
    const auto keyId = advanced_platformer::itemDefinition(itemCatalog, "key").id;
    const advanced_platformer::Actor player = advanced_platformer::composePlayer(gameCatalogs, 0);
    const auto firstLevel = advanced_platformer::composeLevel(
        advanced_platformer::loadRoomPieceCatalog("tests/fixtures/rooms/opening/pieces.json"),
        1,
        1,
        0,
        gameCatalogs,
        player);
    const auto secondLevel = advanced_platformer::composeLevel(
        advanced_platformer::loadRoomPieceCatalog("tests/fixtures/rooms/finish/pieces.json"),
        1,
        1,
        0,
        gameCatalogs,
        player);
    REQUIRE(firstLevel.world.pickups().front().stack.item == keyId);
    REQUIRE(firstLevel.world.itemDefinition(keyId).name == "Session key");
    REQUIRE(secondLevel.world.itemDefinition(keyId).name == "Session key");
}

TEST_CASE("Unknown pickup definitions identify their placement", "[app][pickups]")
{
    const auto pieces = advanced_platformer::loadRoomPieceCatalog(
        "tests/fixtures/rooms/unknown_pickup/pieces.json");
    const auto gameCatalogs =
        advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize);
    REQUIRE_THROWS_WITH(
        advanced_platformer::composeLevel(
            pieces, 1, 1, 0, gameCatalogs, advanced_platformer::composePlayer(gameCatalogs, 0)),
        Catch::Matchers::ContainsSubstring("Level 1 (seed 1): pickup '") &&
            Catch::Matchers::ContainsSubstring("unknown pickup definition 'missing'"));
}

TEST_CASE("Level exit placement combines a definition with its requirement", "[app][exits]")
{
    const auto pieces =
        advanced_platformer::loadRoomPieceCatalog("tests/fixtures/rooms/opening/pieces.json");
    const auto gameCatalogs =
        advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize);
    const auto gameLevel = advanced_platformer::composeLevel(
        pieces, 1, 1, 7, gameCatalogs, advanced_platformer::composePlayer(gameCatalogs, 7));
    const auto& exit = gameLevel.world.exit();
    if (!exit || !exit->sprite)
    {
        throw std::logic_error("Missing fixture exit");
    }
    REQUIRE(exit->bounds.size == glm::vec2{12, 24});
    REQUIRE(exit->sprite->textureId == 7);
    REQUIRE(exit->requirement.has_value());
}

TEST_CASE("Exit item references resolve through the item catalog", "[app][pickups]")
{
    const auto pieces =
        advanced_platformer::loadRoomPieceCatalog("tests/fixtures/rooms/unknown_item/pieces.json");
    const auto gameCatalogs =
        advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize);
    REQUIRE_THROWS_WITH(
        advanced_platformer::composeLevel(
            pieces, 1, 1, 0, gameCatalogs, advanced_platformer::composePlayer(gameCatalogs, 0)),
        Catch::Matchers::ContainsSubstring("Level 1 (seed 1):"));
    REQUIRE_THROWS_WITH(
        advanced_platformer::composeLevel(
            pieces, 1, 1, 0, gameCatalogs, advanced_platformer::composePlayer(gameCatalogs, 0)),
        Catch::Matchers::ContainsSubstring("exit: unknown item 'missing'"));
}

TEST_CASE("Unknown exit definitions identify their placement", "[app][exits]")
{
    const auto pieces =
        advanced_platformer::loadRoomPieceCatalog("tests/fixtures/rooms/unknown_exit/pieces.json");
    const auto gameCatalogs =
        advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize);
    REQUIRE_THROWS_WITH(
        advanced_platformer::composeLevel(
            pieces, 1, 1, 0, gameCatalogs, advanced_platformer::composePlayer(gameCatalogs, 0)),
        Catch::Matchers::ContainsSubstring(
            "Level 1 (seed 1): exit: unknown exit definition 'missing'"));
}
