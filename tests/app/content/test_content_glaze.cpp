#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <glm/vec2.hpp>
#include "content/content_glaze.hpp"

namespace advanced_platformer
{
    // A file shape for testing the shared reading rules. Glaze reflects only types with
    // linkage, so these cannot go in an anonymous namespace.
    struct TestSettingsJson
    {
        int count = 0;
        std::optional<int> spare;
        glm::vec2 size{};
    };

    struct TestDefaultsJson
    {
        float speed = 3.0F;
        float height = 5.0F;
    };

    struct TestFileJson
    {
        TestSettingsJson settings;
        std::optional<WithDefaults<TestDefaultsJson>> tuning;
    };
}

namespace
{
    using advanced_platformer::readContent;
    using advanced_platformer::TestFileJson;

    TestFileJson read(const std::string& text)
    {
        return readContent<TestFileJson>(text, "test.json");
    }
}

TEST_CASE("Content reads fill optional members only when given", "[app][content][json]")
{
    const auto absent = read(R"({"settings":{"count":2,"size":[3,4]}})");
    REQUIRE(absent.settings.count == 2);
    REQUIRE_FALSE(absent.settings.spare.has_value());
    REQUIRE(absent.settings.size == glm::vec2{3, 4});
    REQUIRE_FALSE(absent.tuning.has_value());

    // A null optional reads as one left out.
    REQUIRE_FALSE(
        read(R"({"settings":{"count":2,"spare":null,"size":[3,4]}})").settings.spare.has_value());

    // An object read with defaults keeps the C++ default of every member it leaves out.
    const auto tuned = read(R"({"settings":{"count":2,"size":[3,4]},"tuning":{"speed":7}})");
    REQUIRE(tuned.tuning.has_value());
    const advanced_platformer::TestDefaultsJson defaults;
    REQUIRE(
        tuned.tuning
            .value_or(advanced_platformer::WithDefaults<advanced_platformer::TestDefaultsJson>{})
            .get()
            .speed == 7.0F);
    REQUIRE(
        tuned.tuning
            .value_or(advanced_platformer::WithDefaults<advanced_platformer::TestDefaultsJson>{})
            .get()
            .height == defaults.height);
}

TEST_CASE("Content reads point at what is wrong", "[app][content][json]")
{
    const auto rejects = [](const std::string& text, const std::string& message)
    { REQUIRE_THROWS_WITH(read(text), message); };
    rejects(
        "{\"settings\":{\"count\":2,\"size\":[3,4],\"cuont\":1}}",
        "test.json: line 1, column 37: unknown field 'cuont'");
    rejects(
        "{\"settings\":{\"count\":2,\"size\":[3]}}",
        "test.json: line 1, column 31: expected two numbers, [x, y]");
    rejects(
        "{\"settings\":{\"count\":2.5,\"size\":[3,4]}}",
        "test.json: line 1, column 22: invalid number '2.5'");
    rejects(
        "{\n  \"settings\": {\"size\": [3, 4]}\n}",
        "test.json: line 2, column 30: missing 'count'");
    rejects(
        "{\"settings\":{\"count\":2,\"size\":[3,4]},\"tuning\":{\"sped\":1}}",
        "test.json: line 1, column 48: unknown field 'sped'");
}

TEST_CASE("Text that is not JSON is reported where its syntax breaks", "[app][content][json]")
{
    REQUIRE_THROWS_WITH(
        read("{\n  \"settings\": {\n"),
        Catch::Matchers::StartsWith("test.json: line 3, column 1: invalid JSON: "));
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
