#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <limits>
#include <stdexcept>
#include <string>
#include "content/item_catalog.hpp"
#include "advanced_platformer/inventory/item.hpp"
#include "support/json_document.hpp"

namespace
{
    tests::Json itemData()
    {
        return tests::parseJson(R"({"items":{"herb":{
            "name":"Healing herb","maximumStack":4,
            "icon":{"position":[12,8],"size":[8,12]},"effect":"heal","effectAmount":3
        }}})");
    }
}

TEST_CASE("Item JSON resolves custom names to stable runtime IDs", "[app][items][json]")
{
    const auto catalog =
        advanced_platformer::parseItemCatalog(tests::dumpJson(itemData()), "items.json");
    const auto stack = advanced_platformer::composeItemStack(catalog, {"herb", 2});
    REQUIRE(stack.item > 0);
    REQUIRE(stack.item == advanced_platformer::itemDefinition(catalog, "herb").id);
    REQUIRE(stack.quantity == 2);
    const auto items = advanced_platformer::composeItems(catalog, 6);
    REQUIRE(items.size() == 1);
    REQUIRE(items[0].name == "Healing herb");
    REQUIRE(items[0].maximumStack == 4);
    REQUIRE(items[0].effect == advanced_platformer::ItemEffect::Heal);
    REQUIRE(items[0].effectAmount == 3);
    REQUIRE(items[0].icon.textureId == 6);
    REQUIRE(items[0].icon.size == glm::vec2{8, 12});
    REQUIRE_THROWS_AS(
        advanced_platformer::composeItemStack(catalog, {"missing", 1}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        advanced_platformer::composeItemStack(catalog, {"herb", 0}), std::invalid_argument);
}

TEST_CASE("Item JSON names where a malformed file goes wrong", "[app][items][json]")
{
    // Shape errors come from reading the file, so they name a line and column and quote the
    // key or value found there.
    auto itemJson = itemData();
    auto& item = itemJson["items"]["herb"];
    std::string expected;
    SECTION("Authored IDs are not supported")
    {
        item["id"] = 17;
        expected = "unknown field 'id'";
    }
    SECTION("Unknown field")
    {
        item["maximimStack"] = 1;
        expected = "unknown field 'maximimStack'";
    }
    SECTION("Unknown effect")
    {
        item["effect"] = "magic";
        expected = "unknown value 'magic'; expected none or heal";
    }
    SECTION("Wrong name type")
    {
        item["name"] = 1;
        expected = "expected text, found '1'";
    }
    SECTION("Fractional stack size")
    {
        item["maximumStack"] = 4.5;
        expected = "invalid number '4.5'";
    }
    SECTION("Bad icon shape")
    {
        item["icon"]["size"] = tests::numbers({8});
        expected = "expected two numbers, [x, y]";
    }
    SECTION("Missing stack size")
    {
        tests::eraseKey(item, "maximumStack");
        expected = "missing 'maximumStack'";
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseItemCatalog(tests::dumpJson(itemJson), "items.json"),
        Catch::Matchers::StartsWith("items.json: line 1, column ") &&
            Catch::Matchers::EndsWith(expected));
}

TEST_CASE("Item JSON names the item a rule rejects", "[app][items][json]")
{
    // Rule errors come from validating what was read, so they name the item's path.
    auto itemJson = itemData();
    auto& item = itemJson["items"]["herb"];
    SECTION("Zero capacity")
    {
        item["maximumStack"] = 0;
    }
    SECTION("No healing")
    {
        item["effectAmount"] = 0;
    }
    SECTION("No effect with amount")
    {
        item["effect"] = "none";
    }
    SECTION("Negative source position")
    {
        item["icon"]["position"] = tests::numbers({-1, 0});
    }
    SECTION("Zero display size")
    {
        item["icon"]["displaySize"] = tests::numbers({0, 8});
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseItemCatalog(tests::dumpJson(itemJson), "items.json"),
        Catch::Matchers::StartsWith("items.json: items.herb"));
}

TEST_CASE("Item definitions are validated without JSON", "[app][items][validation]")
{
    auto catalog = advanced_platformer::parseItemCatalog(tests::dumpJson(itemData()), "fixture");
    catalog.definitions.at("herb").icon.size.x = std::numeric_limits<float>::infinity();
    REQUIRE_THROWS_AS(advanced_platformer::validateItemCatalog(catalog), std::invalid_argument);
    REQUIRE_THROWS_AS(
        advanced_platformer::loadItemCatalog("tests/fixtures/catalogs/missing-items.json"),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        advanced_platformer::parseItemCatalog("not JSON", "broken"), std::invalid_argument);
}

TEST_CASE(
    "Item names receive distinct generated IDs even with matching display names",
    "[app][items][json]")
{
    auto itemJson = itemData();
    itemJson["items"]["other_herb"] = itemJson["items"]["herb"];
    const auto catalog =
        advanced_platformer::parseItemCatalog(tests::dumpJson(itemJson), "items.json");
    const auto& first = advanced_platformer::itemDefinition(catalog, "herb");
    const auto& second = advanced_platformer::itemDefinition(catalog, "other_herb");
    REQUIRE(first.id > 0);
    REQUIRE(second.id > 0);
    REQUIRE(first.id != second.id);
    REQUIRE(first.name == second.name);
}
