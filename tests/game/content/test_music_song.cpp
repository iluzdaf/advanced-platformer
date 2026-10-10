#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "advanced_platformer/audio/music.hpp"
#include "content/music_song.hpp"

namespace advanced_platformer
{
    namespace
    {
        const std::string SongJson =
            R"({"tempo":120,"instruments":{"soft":{}},"tracks":{"lead":"soft"},"phrases":{"a":{"notes":[{"pitch":"C4","beats":1},{"pitch":"rest","beats":1}]}},"patterns":{"a":{"beats":2,"tracks":{"lead":"a"}}},"arrangement":["a"]})";
    }
}

TEST_CASE(
    "Music JSON loads named phrases and optional instrument settings",
    "[audio][music][content]")
{
    const auto song =
        advanced_platformer::parseMusicSong(advanced_platformer::SongJson, "song.json");
    REQUIRE(song.instruments.at("soft").wave == "sine");
    REQUIRE(song.phrases.at("a").notes.front().volume == 1);
    REQUIRE(song.arrangement == std::vector<std::string>{"a"});
    REQUIRE(advanced_platformer::renderMusic(song).samples.size() == 44100);
}

TEST_CASE("Music JSON diagnoses references with the source filename", "[audio][music][content]")
{
    auto text = advanced_platformer::SongJson;
    text.replace(text.find("\"lead\":\"soft\""), 13, "\"lead\":\"typo\"");
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseMusicSong(text, "song.json"),
        Catch::Matchers::ContainsSubstring("song.json: tracks.lead"));
}

TEST_CASE("Music JSON rejects missing fields and spelling mistakes", "[audio][music][content]")
{
    REQUIRE_THROWS_AS(
        advanced_platformer::parseMusicSong("{}", "song.json"), std::invalid_argument);
    auto text = advanced_platformer::SongJson;
    text.replace(text.find("\"pitch\""), 7, "\"pich\"");
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseMusicSong(text, "song.json"),
        Catch::Matchers::ContainsSubstring("pich"));
    text = advanced_platformer::SongJson;
    text.replace(text.find("\"C4\""), 4, "\"invalid\"");
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseMusicSong(text, "song.json"),
        Catch::Matchers::ContainsSubstring("phrases.a.notes[0].pitch"));
}

TEST_CASE("Shipped music files have valid composition and references", "[audio][music][content]")
{
    bool found = false;
    for (const auto& file : std::filesystem::directory_iterator("assets/music"))
    {
        if (file.path().extension() == ".json")
        {
            found = true;
            INFO(file.path().string());
            REQUIRE_NOTHROW(advanced_platformer::loadMusicSong(file.path()));
        }
    }
    REQUIRE(found);
}
