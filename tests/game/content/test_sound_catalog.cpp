#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <stdexcept>

#include "content/sound_catalog.hpp"

TEST_CASE("A sound catalog renders named jsfxr patches with optional defaults", "[audio][content]")
{
    const auto sounds = advanced_platformer::parseSoundCatalog(
        R"({"sounds":{"test":{"wave_type":2,"p_env_attack":0.01,"p_env_decay":0.1,"sample_rate":22050,"sample_size":16,"oldParams":true}}})",
        "sounds.json");
    REQUIRE(sounds.size() == 1);
    REQUIRE(sounds.at("test")->sampleRate == 22050);
    REQUIRE_FALSE(sounds.at("test")->samples.empty());
    REQUIRE(advanced_platformer::parseSoundCatalog(R"({"sounds":{}})", "sounds.json").empty());
}

TEST_CASE("Invalid patches identify the catalog and sound", "[audio][content]")
{
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseSoundCatalog(
            R"({"sounds":{"bad":{"p_freq_ramp":2}}})", "sounds.json"),
        Catch::Matchers::ContainsSubstring("sounds.json: sounds.bad: p_freq_ramp"));
}

TEST_CASE("A sound catalog rejects field typos and empty names", "[audio][content]")
{
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseSoundCatalog(
            R"({"sounds":{"bad":{"p_env_decya":0.2}}})", "sounds.json"),
        Catch::Matchers::ContainsSubstring("p_env_decya"));
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseSoundCatalog(R"({"sounds":{"":{}}})", "sounds.json"),
        Catch::Matchers::ContainsSubstring("non-empty name"));
    REQUIRE_THROWS_AS(
        advanced_platformer::parseSoundCatalog("{}", "sounds.json"), std::invalid_argument);
}

TEST_CASE("A missing sound catalog fails at the file boundary", "[audio][content]")
{
    REQUIRE_THROWS_AS(
        advanced_platformer::loadSoundCatalog("tests/fixtures/catalogs/no-sounds.json"),
        std::invalid_argument);
}
