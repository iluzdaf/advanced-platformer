#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

#include "advanced_platformer/audio/music.hpp"
#include "advanced_platformer/audio/sound_mixer.hpp"

namespace advanced_platformer
{
    namespace
    {
        MusicSong fixtureSong()
        {
            MusicSong song;
            song.instruments["tone"] = {
                .wave = "triangle",
                .attack = 0,
                .decay = 0,
                .sustain = 1,
                .release = 0,
                .gain = 0.2};
            song.tracks = {{"lead", "tone"}};
            song.phrases["a"].notes = {{.pitch = "A4", .beats = 0.25}};
            song.patterns["a"] = {.beats = 0.5, .tracks = {{"lead", "a"}}};
            song.arrangement = {"a"};
            return song;
        }
    }
}

TEST_CASE("Music arranges reusable phrases on the sample clock", "[audio][music]")
{
    auto song = advanced_platformer::fixtureSong();
    song.arrangement = {"a", "a"};
    const auto sound = advanced_platformer::renderMusic(song);
    REQUIRE(sound.sampleRate == advanced_platformer::SoundOutputRate);
    REQUIRE(sound.samples.size() == 22050);
    REQUIRE(
        std::equal(
            sound.samples.begin(), sound.samples.begin() + 11025, sound.samples.begin() + 11025));
    REQUIRE(std::ranges::any_of(sound.samples, [](float sample) { return sample != 0; }));
    REQUIRE(
        std::ranges::all_of(
            sound.samples.begin() + 5513,
            sound.samples.begin() + 11025,
            [](float sample) { return sample == 0; }));
}

TEST_CASE("Music solos tracks and mixes their samples", "[audio][music]")
{
    auto song = advanced_platformer::fixtureSong();
    song.tracks["bass"] = "tone";
    song.patterns["a"].tracks["bass"] = "a";
    const auto mix = advanced_platformer::renderMusic(song);
    const auto lead = advanced_platformer::renderMusic(song, "lead");
    const auto bass = advanced_platformer::renderMusic(song, "bass");
    for (std::size_t index = 0; index < mix.samples.size(); ++index)
    {
        REQUIRE(mix.samples[index] == lead.samples[index] + bass.samples[index]);
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::renderMusic(song, "typo"),
        Catch::Matchers::ContainsSubstring("unknown solo track"));
}

TEST_CASE("Music gates release held notes and wraps release tails into the loop", "[audio][music]")
{
    auto song = advanced_platformer::fixtureSong();
    auto& instrument = song.instruments.at("tone");
    instrument.wave = "square";
    instrument.attack = 0.01;
    instrument.release = 0.02;
    auto& notes = song.phrases.at("a").notes;
    notes = {{.pitch = "rest", .beats = 0.25}, {.pitch = "A4", .beats = 0.25, .gate = 0.125}};
    const auto gated = advanced_platformer::renderMusic(song);
    REQUIRE(gated.samples.front() == 0);
    REQUIRE(std::abs(gated.samples[6000]) > 0.05F);
    REQUIRE(gated.samples[10000] == 0);
    notes.back().gate.reset();
    const auto held = advanced_platformer::renderMusic(song);
    REQUIRE(std::abs(held.samples[5]) > 0.05F);
    REQUIRE(held.samples[1500] == 0);
}

TEST_CASE("Music sustains after attack and decay until the gate ends", "[audio][music]")
{
    auto song = advanced_platformer::fixtureSong();
    auto& instrument = song.instruments.at("tone");
    instrument.wave = "square";
    instrument.attack = 0.01;
    instrument.decay = 0.01;
    instrument.sustain = 0.25;
    const auto sound = advanced_platformer::renderMusic(song);
    REQUIRE(sound.samples.front() == 0);
    REQUIRE(std::abs(sound.samples[220]) < std::abs(sound.samples[440]));
    REQUIRE(std::abs(sound.samples[1500]) == Catch::Approx(0.05).margin(0.001));
    REQUIRE(std::abs(sound.samples[4000]) == Catch::Approx(0.05).margin(0.001));
}

TEST_CASE("Music renders deterministic noise and clips summed tracks", "[audio][music]")
{
    auto song = advanced_platformer::fixtureSong();
    song.instruments.at("tone").wave = "noise";
    const auto first = advanced_platformer::renderMusic(song);
    REQUIRE(first.samples == advanced_platformer::renderMusic(song).samples);
    REQUIRE(std::ranges::all_of(first.samples, [](float sample) { return std::isfinite(sample); }));
    song.instruments.at("tone").wave = "square";
    song.instruments.at("tone").gain = 1;
    song.tracks["bass"] = "tone";
    song.patterns.at("a").tracks["bass"] = "a";
    const auto loud = advanced_platformer::renderMusic(song);
    REQUIRE(
        std::ranges::all_of(
            loud.samples, [](float sample) { return sample >= -1 && sample <= 1; }));
    REQUIRE(std::ranges::any_of(loud.samples, [](float sample) { return sample == 1; }));
}

TEST_CASE("Music timing rounds absolute beat positions without accumulated drift", "[audio][music]")
{
    auto song = advanced_platformer::fixtureSong();
    song.tempo = 137;
    song.arrangement.assign(20, "a");
    REQUIRE(
        advanced_platformer::renderMusic(song).samples.size() ==
        static_cast<std::size_t>(std::llround(10 * 60.0 / 137 * 44100)));
}

TEST_CASE("Music pitches use concert A and equivalent accidentals", "[audio][music]")
{
    REQUIRE(advanced_platformer::musicPitchFrequency("A4") == 440);
    REQUIRE(advanced_platformer::musicPitchFrequency("A3") == 220);
    REQUIRE(
        advanced_platformer::musicPitchFrequency("C#4") ==
        advanced_platformer::musicPitchFrequency("Db4"));
    for (const auto* pitch : {"", "A", "H4", "C10", "C!4", "c4"})
    {
        REQUIRE_THROWS_AS(advanced_platformer::musicPitchFrequency(pitch), std::invalid_argument);
    }
}

TEST_CASE("Music validates references and phrase placement", "[audio][music]")
{
    auto song = advanced_platformer::fixtureSong();
    SECTION("instrument")
    {
        song.tracks["lead"] = "missing";
    }
    SECTION("track")
    {
        song.patterns.at("a").tracks["missing"] = "a";
    }
    SECTION("phrase")
    {
        song.patterns.at("a").tracks["lead"] = "missing";
    }
    SECTION("pattern")
    {
        song.arrangement = {"missing"};
    }
    SECTION("phrase length")
    {
        song.phrases.at("a").notes.front().beats = 1;
    }
    REQUIRE_THROWS_AS(advanced_platformer::validateMusicSong(song), std::invalid_argument);
}

TEST_CASE("Music bounds duration and rejects invalid timing and envelopes", "[audio][music]")
{
    auto song = advanced_platformer::fixtureSong();
    SECTION("duration")
    {
        song.arrangement.assign(1000, "a");
    }
    SECTION("tempo")
    {
        song.tempo = std::numeric_limits<double>::infinity();
    }
    SECTION("beat")
    {
        song.phrases.at("a").notes.front().beats = 0;
    }
    SECTION("gate")
    {
        song.phrases.at("a").notes.front().gate = 1;
    }
    SECTION("rest gate")
    {
        song.phrases.at("a").notes.front() = {.pitch = "rest", .beats = 0.25, .gate = 0.1};
    }
    SECTION("wave")
    {
        song.instruments.at("tone").wave = "typo";
    }
    SECTION("envelope")
    {
        song.instruments.at("tone").sustain = -1;
    }
    SECTION("empty arrangement")
    {
        song.arrangement.clear();
    }
    SECTION("empty tracks")
    {
        song.tracks.clear();
    }
    REQUIRE_THROWS_AS(advanced_platformer::validateMusicSong(song), std::invalid_argument);
}
