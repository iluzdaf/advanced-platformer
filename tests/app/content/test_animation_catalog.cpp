#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <string>
#include <limits>
#include <stdexcept>
#include "content/animation_catalog.hpp"
#include "content/actor_catalog.hpp"
#include "content/actor_definition.hpp"
#include "content/content_glaze.hpp"
#include "advanced_platformer/render/animation.hpp"
#include "advanced_platformer/render/sprite.hpp"
#include "support/actor_components.hpp"
#include "support/json_document.hpp"

TEST_CASE("Animation JSON preserves frame order timing and looping", "[app][animations]")
{
    auto animationJson = tests::parseJson(
        advanced_platformer::loadContentText("tests/fixtures/catalogs/animations.json"));
    auto& move = animationJson["animations"]["test_actor"]["move"];
    move["frames"].get_array().push_back(
        tests::object({{"position", tests::numbers({8, 0})}, {"size", tests::numbers({8, 12})}}));
    move["frames"].get_array().push_back(
        tests::object({{"position", tests::numbers({0, 0})}, {"size", tests::numbers({8, 12})}}));
    move["frameDuration"] = 0.25;
    const auto catalog = advanced_platformer::parseAnimationCatalog(
        tests::dumpJson(animationJson), "test animations");
    const auto& set = advanced_platformer::animationSet(catalog, "test_actor");
    const auto& clip = advanced_platformer::clipFor(set, advanced_platformer::AnimationName::Move);
    REQUIRE(clip.frames.size() == 3);
    REQUIRE(clip.frameDuration == 0.25F);
    REQUIRE(clip.looping);
    REQUIRE(advanced_platformer::frameAt(clip, 0.25F).position.x == 8);
    REQUIRE(advanced_platformer::frameAt(clip, 0.5F).position.x == 0);
    REQUIRE_FALSE(
        advanced_platformer::clipFor(set, advanced_platformer::AnimationName::Death).looping);
}

TEST_CASE("Animation catalogs reject invalid content with source context", "[app][animations]")
{
    auto animationJson = tests::parseJson(
        advanced_platformer::loadContentText("tests/fixtures/catalogs/animations.json"));
    auto& set = animationJson["animations"]["test_actor"];
    std::string start = "clips.json: animations.test_actor";
    std::string end;
    SECTION("Missing clip")
    {
        tests::eraseKey(set, "death");
        start = "clips.json: line 1, column ";
        end = "missing 'death'";
    }
    SECTION("Unknown clip")
    {
        set["run"] = set["move"];
        start = "clips.json: line 1, column ";
        end = "unknown field 'run'";
    }
    SECTION("Empty frames")
    {
        set["idle"]["frames"] = tests::emptyArray();
    }
    SECTION("Zero duration")
    {
        set["idle"]["frameDuration"] = 0;
    }
    SECTION("Boolean duration")
    {
        set["idle"]["frameDuration"] = true;
        start = "clips.json: line 1, column ";
        end = "invalid number 'true'";
    }
    SECTION("Nonboolean looping")
    {
        set["idle"]["looping"] = 1;
        start = "clips.json: line 1, column ";
        end = "expected true or false";
    }
    SECTION("Invalid rectangle")
    {
        set["idle"]["frames"][0]["size"] = tests::numbers({0, 12});
    }
    SECTION("Negative position")
    {
        set["idle"]["frames"][0]["position"] = tests::numbers({-1, 0});
    }
    SECTION("Vector shape")
    {
        set["idle"]["frames"][0]["position"] = tests::numbers({1});
        start = "clips.json: line 1, column ";
        end = "expected two numbers, [x, y]";
    }
    SECTION("Mixed sizes")
    {
        set["move"]["frames"][0]["size"] = tests::numbers({16, 12});
    }
    SECTION("Unknown field")
    {
        set["idle"]["elapsed"] = 0;
        start = "clips.json: line 1, column ";
        end = "unknown field 'elapsed'";
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseAnimationCatalog(tests::dumpJson(animationJson), "clips.json"),
        Catch::Matchers::StartsWith(start) && Catch::Matchers::EndsWith(end));
}

TEST_CASE("Animation validation also accepts C++ definitions", "[app][animations]")
{
    auto catalog =
        advanced_platformer::loadAnimationCatalog("tests/fixtures/catalogs/animations.json");
    SECTION("Nonfinite timing")
    {
        catalog.at("test_actor").clips.front().frameDuration =
            std::numeric_limits<float>::infinity();
    }
    SECTION("Duplicate clip")
    {
        auto& set = catalog.at("test_actor");
        set.clips.push_back(set.clips.front());
    }
    SECTION("Empty name")
    {
        catalog.emplace("", catalog.at("test_actor"));
    }
    REQUIRE_THROWS_AS(
        advanced_platformer::validateAnimationCatalog(catalog), std::invalid_argument);
}

TEST_CASE("Animation loading reports missing files and unknown sets", "[app][animations]")
{
    REQUIRE_THROWS_AS(
        advanced_platformer::loadAnimationCatalog(
            "tests/fixtures/catalogs/missing-animations.json"),
        std::invalid_argument);
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseAnimationCatalog("{", "broken.json"),
        Catch::Matchers::ContainsSubstring("broken.json:"));
    REQUIRE_THROWS_WITH(
        advanced_platformer::animationSet({}, "missing"),
        Catch::Matchers::ContainsSubstring("unknown animation set 'missing'"));
}

TEST_CASE("Animation domain diagnostics identify the clip and frame", "[app][animations]")
{
    auto animationJson = tests::parseJson(
        advanced_platformer::loadContentText("tests/fixtures/catalogs/animations.json"));
    SECTION("Duration")
    {
        animationJson["animations"]["test_actor"]["move"]["frameDuration"] = 0;
        REQUIRE_THROWS_WITH(
            advanced_platformer::parseAnimationCatalog(
                tests::dumpJson(animationJson), "clips.json"),
            Catch::Matchers::ContainsSubstring("animations.test_actor: move.frameDuration:"));
    }
    SECTION("Rectangle")
    {
        animationJson["animations"]["test_actor"]["move"]["frames"][0]["size"] =
            tests::numbers({0, 12});
        REQUIRE_THROWS_WITH(
            advanced_platformer::parseAnimationCatalog(
                tests::dumpJson(animationJson), "clips.json"),
            Catch::Matchers::ContainsSubstring("animations.test_actor: move.frames[0]:"));
    }
}

TEST_CASE("Actors have independent playback of shared animation definitions", "[app][animations]")
{
    const auto animations =
        advanced_platformer::loadAnimationCatalog("tests/fixtures/catalogs/animations.json");
    const auto actors = advanced_platformer::parseActorCatalog(
        R"({"player":"hero","actors":{"hero":{"bodySize":[12,20],"movement":{"platformer":{}},"health":3,"inventorySlots":2,"animations":"test_actor"}}})",
        "actors.json",
        animations);
    const auto& definition = advanced_platformer::actorDefinition(actors, "hero");
    auto first = advanced_platformer::composeActor(definition, animations, 7);
    auto second = advanced_platformer::composeActor(definition, animations, 7);
    advanced_platformer::updateAnimation(
        tests::component<advanced_platformer::Animator>(first),
        tests::component<advanced_platformer::Sprite>(first),
        advanced_platformer::AnimationName::Idle,
        0.1F);
    REQUIRE(tests::component<advanced_platformer::Animator>(first).elapsed == 0.1F);
    REQUIRE(tests::component<advanced_platformer::Animator>(second).elapsed == 0);
    REQUIRE(tests::component<advanced_platformer::Sprite>(first).textureId == 7);
    REQUIRE(tests::component<advanced_platformer::Sprite>(first).region.size == glm::vec2{8, 12});
}
