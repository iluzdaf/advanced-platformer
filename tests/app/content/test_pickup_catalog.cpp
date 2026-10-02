#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <limits>
#include <stdexcept>
#include <string>
#include "content/item_catalog.hpp"
#include "content/pickup_catalog.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/render/sprite.hpp"
#include "support/json_document.hpp"

TEST_CASE("Pickup definitions compose bounds and optional world sprites", "[app][pickups]")
{
    const auto items = advanced_platformer::loadItemCatalog("tests/fixtures/catalogs/items.json");
    const auto catalog =
        advanced_platformer::loadPickupCatalog("tests/fixtures/catalogs/pickups.json", items);
    const auto key = advanced_platformer::composePickup(
        advanced_platformer::pickupDefinition(catalog, "door_key"), items, 7, {40, 48});
    REQUIRE(key.stack.item == advanced_platformer::itemDefinition(items, "key").id);
    REQUIRE(key.stack.quantity == 1);
    REQUIRE(key.body.bounds.size == glm::vec2{10, 12});
    REQUIRE(advanced_platformer::feetOf(key.body.bounds) == glm::vec2{40, 48});
    REQUIRE_FALSE(key.sprite.has_value());
    const auto medicine = advanced_platformer::composePickup(
        advanced_platformer::pickupDefinition(catalog, "medicine_box"), items, 7, {24, 32});
    if (!medicine.sprite)
    {
        throw std::logic_error("Missing sprite override");
    }
    REQUIRE(medicine.sprite->textureId == 7);
    REQUIRE(medicine.sprite->region.size == glm::vec2{24, 16});
    REQUIRE(medicine.sprite->anchor == advanced_platformer::SpriteAnchor::BodyCenter);
    REQUIRE(
        advanced_platformer::itemDefinition(items, "medicine").icon.region.size == glm::vec2{8, 8});
}

TEST_CASE("Pickup JSON validates every definition including unused entries", "[app][pickups][json]")
{
    const auto items = advanced_platformer::loadItemCatalog("tests/fixtures/catalogs/items.json");
    auto pickupJson = tests::parseJson(
        R"({"pickups":{"unused":{"item":"key","quantity":1,"bodySize":[10,12]}}})");
    auto& definition = pickupJson["pickups"]["unused"];
    std::string start = "pickups.json: pickups.unused";
    std::string end;
    SECTION("Missing body size")
    {
        tests::eraseKey(definition, "bodySize");
        start = "pickups.json: line 1, column ";
        end = "missing 'bodySize'";
    }
    SECTION("Unknown item")
    {
        definition["item"] = "missing";
    }
    SECTION("Nonpositive quantity")
    {
        definition["quantity"] = 0;
    }
    SECTION("Fractional quantity")
    {
        definition["quantity"] = 0.5;
        start = "pickups.json: line 1, column ";
        end = "invalid number '0.5'";
    }
    SECTION("Invalid bounds")
    {
        definition["bodySize"] = tests::numbers({0, 12});
    }
    SECTION("Wrong vector shape")
    {
        definition["bodySize"] = tests::numbers({12});
        start = "pickups.json: line 1, column ";
        end = "expected two numbers, [x, y]";
    }
    SECTION("Unknown field")
    {
        definition["bodySze"] = tests::numbers({12, 12});
        start = "pickups.json: line 1, column ";
        end = "unknown field 'bodySze'";
    }
    SECTION("Invalid sprite")
    {
        definition["sprite"] =
            tests::object({{"position", tests::numbers({0, 0})}, {"size", tests::numbers({0, 8})}});
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parsePickupCatalog(tests::dumpJson(pickupJson), "pickups.json", items),
        Catch::Matchers::StartsWith(start) && Catch::Matchers::EndsWith(end));
}

TEST_CASE("Pickup definitions reject invalid C++ data without JSON", "[app][pickups][validation]")
{
    const auto items = advanced_platformer::loadItemCatalog("tests/fixtures/catalogs/items.json");
    advanced_platformer::PickupDefinition definition;
    definition.stack = {"key", 1};
    definition.bodySize = {10.0F, 12.0F};
    REQUIRE_NOTHROW(advanced_platformer::validatePickupDefinition(definition, items));
    definition.bodySize.x = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_THROWS_AS(
        advanced_platformer::validatePickupDefinition(definition, items), std::invalid_argument);
    REQUIRE_THROWS_AS(
        advanced_platformer::loadPickupCatalog(
            "tests/fixtures/catalogs/missing-pickups.json", items),
        std::invalid_argument);
}
