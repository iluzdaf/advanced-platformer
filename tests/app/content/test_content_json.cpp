#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <limits>
#include <string>
#include "content/content_json.hpp"
#include <glm/vec2.hpp>

TEST_CASE("Required JSON reads name the field they reject", "[app][content][json]")
{
    const auto object = nlohmann::json::parse(R"({"quantity":"bad"})");
    REQUIRE_THROWS_WITH(
        advanced_platformer::readInteger(object, "quantity", "items.json", "items.key"),
        "items.json: items.key.quantity: expected an integer");
    REQUIRE_THROWS_WITH(
        advanced_platformer::readInteger(object, "missing", "items.json", "items.key"),
        "items.json: items.key: missing 'missing'");
}

TEST_CASE("Optional JSON reads keep defaults only when absent", "[app][content][json]")
{
    glm::vec2 size{8, 12};
    advanced_platformer::readOptionalVector(nlohmann::json::object(), "size", size);
    REQUIRE(size == glm::vec2{8, 12});
    advanced_platformer::readOptionalVector(
        nlohmann::json::parse(R"({"size":[2,3]})"), "size", size);
    REQUIRE(size == glm::vec2{2, 3});
    REQUIRE_THROWS_WITH(
        advanced_platformer::readOptionalVector(nlohmann::json{{"size", nullptr}}, "size", size),
        "size: expected [x, y]");
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
