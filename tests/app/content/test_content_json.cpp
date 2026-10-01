#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <limits>
#include <string>
#include "content/content_json.hpp"
#include <glm/vec2.hpp>

TEST_CASE("Required and optional JSON reads use the same diagnostics", "[app][content][json]")
{
    const auto object = nlohmann::json::parse(R"({"quantity":"bad"})");
    int quantity = 7;
    REQUIRE_THROWS_WITH(
        advanced_platformer::readInteger(object, "quantity", "items.json", "items.key"),
        "items.json: items.key.quantity: expected an integer");
    REQUIRE_THROWS_WITH(
        advanced_platformer::readOptionalInteger(
            object, "quantity", quantity, "items.json", "items.key"),
        "items.json: items.key.quantity: expected an integer");
    REQUIRE(quantity == 7);
    REQUIRE_THROWS_WITH(
        advanced_platformer::readInteger(object, "missing", "items.json", "items.key"),
        "items.json: items.key: missing 'missing'");
}

TEST_CASE("Optional JSON reads keep defaults only when absent", "[app][content][json]")
{
    const auto empty = nlohmann::json::object();
    int count = 7;
    float speed = 3;
    bool enabled = true;
    std::string name = "default";
    glm::vec2 size{8, 12};
    advanced_platformer::readOptionalInteger(empty, "count", count);
    advanced_platformer::readOptionalNumber(empty, "speed", speed);
    advanced_platformer::readOptionalBoolean(empty, "enabled", enabled);
    advanced_platformer::readOptionalText(empty, "name", name);
    advanced_platformer::readOptionalVector(empty, "size", size);
    REQUIRE(count == 7);
    REQUIRE(speed == 3);
    REQUIRE(enabled);
    REQUIRE(name == "default");
    REQUIRE(size == glm::vec2{8, 12});
    const auto values = nlohmann::json::parse(
        R"({"count":2,"speed":4.5,"enabled":false,"name":"new","size":[2,3]})");
    advanced_platformer::readOptionalInteger(values, "count", count);
    advanced_platformer::readOptionalNumber(values, "speed", speed);
    advanced_platformer::readOptionalBoolean(values, "enabled", enabled);
    advanced_platformer::readOptionalText(values, "name", name);
    advanced_platformer::readOptionalVector(values, "size", size);
    REQUIRE(count == 2);
    REQUIRE(speed == 4.5F);
    REQUIRE_FALSE(enabled);
    REQUIRE(name == "new");
    REQUIRE(size == glm::vec2{2, 3});
    REQUIRE_THROWS_WITH(
        advanced_platformer::readOptionalInteger(
            nlohmann::json{{"count", nullptr}}, "count", count),
        "count: expected an integer");
}

TEST_CASE(
    "Shared JSON values reject invalid types ranges and nonfinite numbers",
    "[app][content][json]")
{
    REQUIRE_THROWS_WITH(
        advanced_platformer::jsonInteger(4294967297LL), "integer is outside the supported range");
    REQUIRE_THROWS_WITH(
        advanced_platformer::jsonInteger(-4294967295LL), "integer is outside the supported range");
    REQUIRE_THROWS_WITH(advanced_platformer::jsonInteger(true), "expected an integer");
    REQUIRE_THROWS_WITH(advanced_platformer::jsonNumber(true), "expected a number");
    REQUIRE_THROWS_WITH(
        advanced_platformer::jsonNumber(std::numeric_limits<double>::infinity()),
        "number must be finite");
    REQUIRE_THROWS_WITH(advanced_platformer::jsonBoolean(1), "expected true or false");
    REQUIRE_THROWS_WITH(advanced_platformer::jsonText(1), "expected text");
    REQUIRE_THROWS_WITH(
        advanced_platformer::jsonVector(nlohmann::json::parse("[1,false]"), "file.json", "size"),
        "file.json: size[1]: expected a number");
    REQUIRE_THROWS_WITH(
        advanced_platformer::jsonSprite(
            nlohmann::json::parse(R"({"position":[0,0],"size":[8,8],"anchor":"wrong"})"),
            "file.json",
            "icon"),
        Catch::Matchers::ContainsSubstring("file.json: icon.anchor:"));
}

TEST_CASE(
    "A content file that cannot be opened is reported by its full path",
    "[app][content][json]")
{
    const std::filesystem::path missing = "missing-content.json";
    REQUIRE_THROWS_WITH(
        advanced_platformer::loadContentText(missing),
        Catch::Matchers::ContainsSubstring(std::filesystem::absolute(missing).string()));
}
