#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include <catch2/matchers/catch_matchers_string.hpp>
#include <glm/vec2.hpp>

#include "content/camera_settings.hpp"
#include "support/json_document.hpp"

TEST_CASE("Camera settings read the dead zone", "[app][content][json]")
{
    const auto camera =
        advanced_platformer::parseCameraSettings(R"({"deadZone": [80, 45]})", "camera.json");

    REQUIRE(camera.deadZone == glm::vec2{80.0F, 45.0F});
}

TEST_CASE("The camera dead zone is positive and fits in the view", "[app][content][json]")
{
    auto cameraJson = tests::parseJson(R"({"deadZone":[80,45]})");
    SECTION("Missing")
    {
        tests::eraseKey(cameraJson, "deadZone");
    }
    SECTION("Empty")
    {
        cameraJson["deadZone"] = tests::numbers({0, 45});
    }
    SECTION("Wider than the view")
    {
        cameraJson["deadZone"] = tests::numbers({321, 45});
    }
    SECTION("Taller than the view")
    {
        cameraJson["deadZone"] = tests::numbers({80, 181});
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseCameraSettings(tests::dumpJson(cameraJson), "camera.json"),
        Catch::Matchers::ContainsSubstring("camera.json:"));
}

TEST_CASE("Camera settings reject field typos", "[app][content][json]")
{
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseCameraSettings(
            R"({"deadZone": [80, 45], "deadZonee": [1, 1]})", "camera.json"),
        Catch::Matchers::ContainsSubstring("camera.json:"));
}

TEST_CASE("Missing camera settings are rejected at the file boundary", "[app][content][json]")
{
    REQUIRE_THROWS_AS(
        advanced_platformer::loadCameraSettings("tests/fixtures/catalogs/does_not_exist.json"),
        std::invalid_argument);
}
