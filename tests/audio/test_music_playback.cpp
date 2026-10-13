#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "advanced_platformer/audio/audio_playback.hpp"
#include "advanced_platformer/audio/music.hpp"
#include "advanced_platformer/audio/music_playback.hpp"
#include "advanced_platformer/audio/sound_mixer.hpp"
#include "advanced_platformer/audio/sound_patch.hpp"

namespace advanced_platformer
{
    namespace
    {
        MusicParts fixtureParts()
        {
            return {
                400,
                {{"backing", {{0.1F, 0.2F, 0.3F}, SoundOutputRate}},
                 {"npc_a", {{0.2F, 0.3F, 0.4F}, SoundOutputRate}},
                 {"npc_b", {{0.05F, 0.05F, 0.05F}, SoundOutputRate}}}};
        }

        void advance(MusicPlayback& playback, std::size_t frames)
        {
            std::vector<float> output(frames);
            playback.mix(output);
        }
    }
}

TEST_CASE(
    "Prepared music parts preserve arrangement length and solo samples",
    "[audio][parts][render]")
{
    using namespace advanced_platformer;
    MusicSong song;
    song.instruments = {{"tone", {.gain = 0.1}}};
    song.tracks = {{"backing", "tone"}, {"npc", "tone"}};
    song.phrases = {{"low", {{{"C3", 1, {}, 1}}}}, {"high", {{{"G4", 1, {}, 1}}}}};
    song.patterns = {{"opening", {1, {{"backing", "low"}}}}, {"answer", {1, {{"npc", "high"}}}}};
    song.arrangement = {"opening", "answer"};
    const auto parts = renderMusicParts(song);
    REQUIRE(parts.tempo == song.tempo);
    REQUIRE(parts.tracks.size() == 2);
    for (const auto& [name, sound] : parts.tracks)
    {
        REQUIRE(sound.samples == renderMusic(song, name).samples);
        REQUIRE(sound.samples.size() == SoundOutputRate);
        REQUIRE(sound.sampleRate == SoundOutputRate);
    }
}

TEST_CASE(
    "Music parts start with backing alone and join at the shared position",
    "[audio][parts][clock]")
{
    using namespace advanced_platformer;
    MusicPlayback playback(fixtureParts(), "backing", 1, 0);
    std::array<float, 2> output{};
    playback.mix(output);
    REQUIRE(output == std::array{0.1F, 0.2F});
    REQUIRE(playback.setPartActive("npc_a", true));
    REQUIRE(playback.setPartActive("npc_b", true));
    advance(playback, 6615 - 3);
    output = {};
    playback.mix(output);
    REQUIRE(output[0] == 0.3F);
    REQUIRE(output[1] == Catch::Approx(0.35F));
    REQUIRE(playback.setPartActive("npc_a", false));
    advance(playback, 6615 - 1);
    output = {};
    playback.mix(output);
    REQUIRE(output[0] == Catch::Approx(0.15F));
    REQUIRE(output[1] == Catch::Approx(0.25F));
}

TEST_CASE(
    "Music part fades take the configured sample count at the bar boundary",
    "[audio][parts][clock]")
{
    using namespace advanced_platformer;
    auto parts = fixtureParts();
    parts.tracks.at("backing").samples = {0};
    parts.tracks.at("npc_a").samples = {0.4F};
    parts.tracks.at("npc_b").samples = {0};
    MusicPlayback playback(std::move(parts), "backing", 1, 4.0 / SoundOutputRate);
    REQUIRE(playback.setPartActive("npc_a", true));
    advance(playback, 6615);
    std::array<float, 5> output{};
    playback.mix(output);
    for (std::size_t i = 0; i < output.size(); ++i)
    {
        REQUIRE(output[i] == Catch::Approx(static_cast<float>(i) * 0.1F));
    }
    REQUIRE(playback.setPartActive("npc_a", false));
    advance(playback, 6615 - output.size());
    output = {};
    playback.mix(output);
    for (std::size_t i = 0; i < output.size(); ++i)
    {
        REQUIRE(output[i] == Catch::Approx(0.4F - static_cast<float>(i) * 0.1F).margin(0.000001F));
    }
}

TEST_CASE(
    "Music part requests coalesce without postponing a pending activation",
    "[audio][parts][clock]")
{
    using namespace advanced_platformer;
    MusicPlayback playback(fixtureParts(), "backing", 1, 0);
    REQUIRE(playback.setPartActive("npc_a", true));
    advance(playback, 5000);
    REQUIRE(playback.setPartActive("npc_a", true));
    advance(playback, 1615);
    std::array<float, 1> output{};
    playback.mix(output);
    REQUIRE(output[0] == Catch::Approx(0.3F));
    REQUIRE(playback.setPartActive("npc_b", true));
    REQUIRE(playback.setPartActive("npc_b", false));
    advance(playback, 6614);
    output = {};
    playback.mix(output);
    REQUIRE(output[0] == Catch::Approx(0.3F));
}

TEST_CASE(
    "Music bar rounding stays accurate with fractional frame durations",
    "[audio][parts][clock]")
{
    using namespace advanced_platformer;
    auto parts = fixtureParts();
    parts.tempo = 137;
    MusicPlayback playback(std::move(parts), "backing", 1, 0);
    std::size_t position = 0;
    for (std::size_t bar = 1; bar <= 10; ++bar)
    {
        const auto boundary = static_cast<std::size_t>(
            std::llround(static_cast<double>(bar) * 60 / 137 * SoundOutputRate));
        REQUIRE(playback.setPartActive("npc_b", bar % 2 != 0));
        advance(playback, boundary - position - 1);
        std::array<float, 2> output{};
        playback.mix(output);
        const auto source = fixtureParts().tracks.at("backing").samples;
        REQUIRE(
            output[0] == Catch::Approx(source[(boundary - 1) % 3] + (bar % 2 == 0 ? 0.05F : 0.0F)));
        REQUIRE(output[1] == Catch::Approx(source[boundary % 3] + (bar % 2 != 0 ? 0.05F : 0.0F)));
        position = boundary + 1;
    }
}

TEST_CASE("A part requested exactly at a bar waits for the following bar", "[audio][parts][clock]")
{
    using namespace advanced_platformer;
    MusicPlayback playback(fixtureParts(), "backing", 1, 0);
    advance(playback, 6615);
    REQUIRE(playback.setPartActive("npc_a", true));
    std::array<float, 1> output{};
    playback.mix(output);
    REQUIRE(output[0] == 0.1F);
    advance(playback, 6614);
    output = {};
    playback.mix(output);
    REQUIRE(output[0] == Catch::Approx(0.3F));
}

TEST_CASE("Music part scheduling is independent of callback block size", "[audio][parts][clock]")
{
    using namespace advanced_platformer;
    MusicPlayback whole(fixtureParts(), "backing", 1, 0.05);
    MusicPlayback split(fixtureParts(), "backing", 1, 0.05);
    REQUIRE(whole.setPartActive("npc_a", true));
    REQUIRE(split.setPartActive("npc_a", true));
    std::vector<float> expected(14000);
    std::vector<float> actual(expected.size());
    whole.mix(expected);
    for (std::size_t offset = 0; offset < actual.size(); offset += 37)
    {
        const auto count = std::min(std::size_t{37}, actual.size() - offset);
        split.mix(std::span(actual).subspan(offset, count));
    }
    REQUIRE(actual == expected);
}

TEST_CASE(
    "Music part playback preserves sound effects and stops with the device queue",
    "[audio][parts][playback]")
{
    using namespace advanced_platformer;
    AudioPlayback playback(std::make_unique<MusicPlayback>(fixtureParts(), "backing", 1, 0));
    REQUIRE(playback.play(
        std::make_shared<const SoundBuffer>(SoundBuffer{{0.2F, 0.2F}, SoundOutputRate})));
    std::array<float, 2> output{};
    playback.render(output);
    REQUIRE(output[0] == Catch::Approx(0.3F));
    REQUIRE(output[1] == Catch::Approx(0.4F));
    REQUIRE(playback.stop());
    playback.render(output);
    REQUIRE(output == std::array{0.0F, 0.0F});
}

TEST_CASE(
    "Music part controls reject invalid names and keep backing continuous",
    "[audio][parts][controls]")
{
    using namespace advanced_platformer;
    MusicPlayback playback(fixtureParts(), "backing");
    REQUIRE_FALSE(playback.setPartActive("backing", false));
    REQUIRE_FALSE(playback.setPartActive("missing", true));
    for (int i = 0; i < 64; ++i)
    {
        REQUIRE(playback.setPartActive("npc_a", true));
    }
    REQUIRE_FALSE(playback.setPartActive("npc_a", false));
    advance(playback, 88200);
    std::array<float, 1> output{};
    playback.mix(output);
    REQUIRE(output[0] == Catch::Approx(0.3F));
    REQUIRE(playback.setPartActive("npc_a", false));
    AudioPlayback effectsOnly;
    REQUIRE_FALSE(effectsOnly.setPartActive("npc_a", true));
}

TEST_CASE(
    "Music part controls can be produced while the callback renders",
    "[audio][parts][controls][thread]")
{
    using namespace advanced_platformer;
    MusicPlayback playback(fixtureParts(), "backing", 1, 0.05);
    std::atomic<bool> done{false};
    std::thread callback(
        [&]
        {
            std::array<float, 64> output{};
            while (!done.load(std::memory_order_acquire))
            {
                output = {};
                playback.mix(output);
            }
        });
    for (int i = 0; i < 10000; ++i)
    {
        while (!playback.setPartActive("npc_a", i % 2 == 0))
        {
            std::this_thread::yield();
        }
    }
    done.store(true, std::memory_order_release);
    callback.join();
    std::array<float, 1> output{};
    playback.mix(output);
    REQUIRE(std::isfinite(output[0]));
}

TEST_CASE(
    "Music part preparation rejects incompatible buffers and timing",
    "[audio][parts][validation]")
{
    using namespace advanced_platformer;
    REQUIRE_THROWS_AS(MusicPlayback(fixtureParts(), "missing"), std::invalid_argument);
    REQUIRE_THROWS_AS(MusicPlayback(fixtureParts(), "backing", 0), std::invalid_argument);
    REQUIRE_THROWS_AS(MusicPlayback(fixtureParts(), "backing", 4, -1), std::invalid_argument);
    auto parts = fixtureParts();
    parts.tempo = std::numeric_limits<double>::quiet_NaN();
    REQUIRE_THROWS_AS(MusicPlayback(parts, "backing"), std::invalid_argument);
    parts = fixtureParts();
    parts.tracks.at("npc_a").samples.pop_back();
    REQUIRE_THROWS_AS(MusicPlayback(parts, "backing"), std::invalid_argument);
    parts = fixtureParts();
    parts.tracks.at("npc_a").sampleRate = 22050;
    REQUIRE_THROWS_AS(MusicPlayback(parts, "backing"), std::invalid_argument);
    parts = fixtureParts();
    parts.tracks.at("npc_a").samples[0] = std::numeric_limits<float>::infinity();
    REQUIRE_THROWS_AS(MusicPlayback(parts, "backing"), std::invalid_argument);
}
