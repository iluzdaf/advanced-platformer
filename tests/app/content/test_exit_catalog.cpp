#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>
#include <limits>
#include <stdexcept>
#include "content/exit_catalog.hpp"
#include "content/content_validation.hpp"
#include "content/level_data.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/render/sprite.hpp"

namespace
{
    nlohmann::json exitData()
    {
        return nlohmann::json::parse(R"({"exits":{"gate":{
            "bodySize":[12,24],"sprite":{"position":[8,16],"size":[8,12],"displaySize":[16,24],"anchor":"center"}
        }}})");
    }
}

TEST_CASE("Exit definitions compose independent bounds and sprites", "[app][exits]")
{
    const auto catalog = advanced_platformer::parseExitCatalog(exitData().dump(), "exits.json");
    const auto exit = advanced_platformer::composeExit(
        advanced_platformer::exitDefinition(catalog, "gate"), 7, {40, 48});
    REQUIRE(exit.bounds.size == glm::vec2{12, 24});
    REQUIRE(advanced_platformer::feetOf(exit.bounds) == glm::vec2{40, 48});
    if (!exit.sprite)
    {
        throw std::logic_error("Missing exit sprite");
    }
    REQUIRE(exit.sprite->textureId == 7);
    REQUIRE(exit.sprite->size == glm::vec2{16, 24});
    REQUIRE(exit.sprite->anchor == advanced_platformer::SpriteAnchor::BodyCenter);
    REQUIRE_FALSE(exit.requirement.has_value());
    REQUIRE_FALSE(exit.nextLevel.has_value());
    REQUIRE_FALSE(exit.consumeItem);
}

TEST_CASE(
    "Exit catalog validates unused definitions and rejects placement settings",
    "[app][exits][json]")
{
    auto exitJson = exitData();
    auto& definition = exitJson["exits"]["gate"];
    SECTION("Invalid bounds")
    {
        definition["bodySize"] = {0, 24};
    }
    SECTION("Invalid sprite")
    {
        definition["sprite"]["size"] = {-1, 12};
    }
    SECTION("Missing sprite")
    {
        definition.erase("sprite");
    }
    SECTION("Missing bounds")
    {
        definition.erase("bodySize");
    }
    SECTION("Malformed vector")
    {
        definition["bodySize"] = {12};
    }
    SECTION("Destination belongs to placement")
    {
        definition["nextLevel"] = 2;
    }
    SECTION("Requirement belongs to placement")
    {
        definition["requirement"] = {{"item", "key"}, {"quantity", 1}};
    }
    SECTION("Consumption belongs to placement")
    {
        definition["consumeItem"] = true;
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseExitCatalog(exitJson.dump(), "exits.json"),
        Catch::Matchers::ContainsSubstring("exits.json: exits.gate"));
}

TEST_CASE("Exit definitions and placement names validate without JSON", "[app][exits][validation]")
{
    auto catalog = advanced_platformer::parseExitCatalog(exitData().dump(), "fixture");
    catalog.at("gate").bodySize.x = std::numeric_limits<float>::infinity();
    REQUIRE_THROWS_AS(advanced_platformer::validateExitCatalog(catalog), std::invalid_argument);
    advanced_platformer::ExitPlacement placement;
    REQUIRE_THROWS_WITH(
        advanced_platformer::validateExitSettings(placement),
        "exit.definition: exit definition name cannot be empty");
    REQUIRE_THROWS_AS(
        advanced_platformer::loadExitCatalog("tests/fixtures/catalogs/missing-exits.json"),
        std::invalid_argument);
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseExitCatalog("{\n  \"exits\": {\n", "broken"),
        Catch::Matchers::ContainsSubstring("broken: line 3, column 1: invalid JSON"));
}
