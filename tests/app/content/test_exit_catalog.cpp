#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <limits>
#include <stdexcept>
#include <string>
#include "content/exit_catalog.hpp"
#include "content/content_validation.hpp"
#include "content/level_data.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/render/sprite.hpp"
#include "support/json_document.hpp"

namespace
{
    tests::Json exitData()
    {
        return tests::parseJson(R"({"exits":{"gate":{
            "bodySize":[12,24],"sprite":{"position":[8,16],"size":[8,12],"displaySize":[16,24],"anchor":"center"}
        }}})");
    }
}

TEST_CASE("Exit definitions compose independent bounds and sprites", "[app][exits]")
{
    const auto catalog =
        advanced_platformer::parseExitCatalog(tests::dumpJson(exitData()), "exits.json");
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
    // Shape errors name a line and column; rule errors name the definition.
    auto exitJson = exitData();
    auto& definition = exitJson["exits"]["gate"];
    std::string start = "exits.json: exits.gate";
    std::string end;
    SECTION("Invalid bounds")
    {
        definition["bodySize"] = tests::numbers({0, 24});
    }
    SECTION("Invalid sprite")
    {
        definition["sprite"]["size"] = tests::numbers({-1, 12});
    }
    SECTION("Missing sprite")
    {
        tests::eraseKey(definition, "sprite");
        start = "exits.json: line 1, column ";
        end = "missing 'sprite'";
    }
    SECTION("Missing bounds")
    {
        tests::eraseKey(definition, "bodySize");
        start = "exits.json: line 1, column ";
        end = "missing 'bodySize'";
    }
    SECTION("Malformed vector")
    {
        definition["bodySize"] = tests::numbers({12});
        start = "exits.json: line 1, column ";
        end = "expected two numbers, [x, y]";
    }
    SECTION("Destination belongs to placement")
    {
        definition["nextLevel"] = 2;
        start = "exits.json: line 1, column ";
        end = "unknown field 'nextLevel'";
    }
    SECTION("Requirement belongs to placement")
    {
        definition["requirement"] = tests::object({{"item", "key"}, {"quantity", 1}});
        start = "exits.json: line 1, column ";
        end = "unknown field 'requirement'";
    }
    SECTION("Consumption belongs to placement")
    {
        definition["consumeItem"] = true;
        start = "exits.json: line 1, column ";
        end = "unknown field 'consumeItem'";
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseExitCatalog(tests::dumpJson(exitJson), "exits.json"),
        Catch::Matchers::StartsWith(start) && Catch::Matchers::EndsWith(end));
}

TEST_CASE("Exit definitions and placement names validate without JSON", "[app][exits][validation]")
{
    auto catalog = advanced_platformer::parseExitCatalog(tests::dumpJson(exitData()), "fixture");
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
