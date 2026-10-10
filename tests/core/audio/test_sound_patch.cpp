#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <format>
#include <limits>
#include <stdexcept>
#include <vector>

#include "advanced_platformer/audio/sound_patch.hpp"

TEST_CASE("Sound synthesis matches the seeded jsfxr reference for every waveform", "[audio][synth]")
{
    for (int wave = 0; wave < 4; ++wave)
    {
        advanced_platformer::SoundPatch patch;
        patch.waveType = wave;
        patch.attack = 0.03;
        patch.sustain = 0.05;
        patch.decay = 0.07;
        patch.punch = 0.2;
        patch.baseFrequency = 0.7;
        patch.frequencySlide = -0.2;
        patch.deltaSlide = 0.1;
        patch.vibratoDepth = 0.3;
        patch.vibratoSpeed = 0.5;
        patch.changeAmount = -0.2;
        patch.changeSpeed = 0.95;
        patch.duty = 0.3;
        patch.dutySweep = -0.1;
        patch.repeatSpeed = 0.9;
        patch.phaserOffset = -0.1;
        patch.phaserSweep = 0.02;
        patch.lowPassCutoff = 0.8;
        patch.lowPassSweep = -0.2;
        patch.lowPassResonance = 0.4;
        patch.highPassCutoff = 0.1;
        patch.highPassSweep = 0.1;
        patch.volume = 0.4;
        const auto sound = advanced_platformer::renderSound(patch, 7);
        std::ifstream reference(std::format("tests/fixtures/audio/jsfxr-{}.txt", wave));
        REQUIRE(reference.is_open());
        std::vector<float> expected;
        float sample = 0;
        while (reference >> sample)
        {
            expected.push_back(sample);
        }
        INFO("wave_type " << wave);
        REQUIRE(expected.size() > 100);
        REQUIRE(sound.samples.size() == expected.size());
        for (std::size_t index = 0; index < expected.size(); ++index)
        {
            INFO("sample " << index);
            REQUIRE_THAT(
                sound.samples[index], Catch::Matchers::WithinAbs(expected[index], 0.000001));
        }
    }
}

TEST_CASE("The sound envelope ends at silence and noise is repeatable", "[audio][synth]")
{
    advanced_platformer::SoundPatch patch;
    patch.waveType = 3;
    patch.attack = 0.01;
    patch.sustain = 0.02;
    patch.decay = 0.03;
    const auto first = advanced_platformer::renderSound(patch, 3);
    const auto again = advanced_platformer::renderSound(patch, 3);
    const auto different = advanced_platformer::renderSound(patch, 4);
    REQUIRE(first.samples == again.samples);
    REQUIRE(first.samples != different.samples);
    REQUIRE(first.samples.back() == 0.0F);
    REQUIRE(
        std::ranges::any_of(first.samples, [](float value) { return std::abs(value) > 0.01F; }));
    REQUIRE(std::ranges::all_of(first.samples, [](float value) { return std::isfinite(value); }));
}

TEST_CASE("A zero-length envelope stage is skipped without a non-finite sample", "[audio][synth]")
{
    advanced_platformer::SoundPatch patch;
    patch.attack = 0;
    patch.sustain = 0;
    patch.decay = 0.1;
    const auto sound = advanced_platformer::renderSound(patch);
    REQUIRE_FALSE(sound.samples.empty());
    REQUIRE(std::ranges::all_of(sound.samples, [](float value) { return std::isfinite(value); }));
    REQUIRE(sound.samples.back() == 0.0F);
}

TEST_CASE("A lower exported sample rate averages the reference-rate samples", "[audio][synth]")
{
    advanced_platformer::SoundPatch patch;
    const auto full = advanced_platformer::renderSound(patch);
    for (int rate : {22050, 11025})
    {
        patch.sampleRate = rate;
        const auto reduced = advanced_platformer::renderSound(patch);
        const auto group = static_cast<std::size_t>(44100 / rate);
        REQUIRE(reduced.sampleRate == rate);
        REQUIRE(reduced.samples.size() == full.samples.size() / group);
        for (std::size_t index = 0; index < reduced.samples.size(); ++index)
        {
            double sum = 0;
            for (std::size_t offset = 0; offset < group; ++offset)
            {
                sum += full.samples[index * group + offset];
            }
            REQUIRE_THAT(
                reduced.samples[index],
                Catch::Matchers::WithinAbs(sum / static_cast<double>(group), 0.000001));
        }
    }
}

TEST_CASE("A descending sound stops when it reaches the frequency cutoff", "[audio][synth]")
{
    advanced_platformer::SoundPatch patch;
    patch.baseFrequency = 0.6;
    patch.frequencySlide = -0.8;
    const auto whole = advanced_platformer::renderSound(patch);
    patch.frequencyLimit = 0.5;
    const auto cut = advanced_platformer::renderSound(patch);
    REQUIRE_FALSE(cut.samples.empty());
    REQUIRE(cut.samples.size() < whole.samples.size());
}

TEST_CASE("Invalid sound parameters are rejected before synthesis", "[audio][synth]")
{
    advanced_platformer::SoundPatch patch;
    SECTION("Non-finite")
    {
        patch.decay = std::numeric_limits<double>::infinity();
    }
    SECTION("Unsigned range")
    {
        patch.duty = -0.1;
    }
    SECTION("Signed range")
    {
        patch.frequencySlide = -1.1;
    }
    SECTION("Unknown waveform")
    {
        patch.waveType = 4;
    }
    SECTION("Empty envelope")
    {
        patch.attack = patch.sustain = patch.decay = 0;
    }
    SECTION("Unknown rate")
    {
        patch.sampleRate = 48000;
    }
    SECTION("Unknown sample size")
    {
        patch.sampleSize = 24;
    }
    SECTION("Different parameter format")
    {
        patch.oldParams = false;
    }
    REQUIRE_THROWS_AS(advanced_platformer::renderSound(patch), std::invalid_argument);
}
