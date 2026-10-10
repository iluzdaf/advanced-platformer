#include <catch2/catch_test_macros.hpp>

#include <array>

#include "advanced_platformer/audio/sound_mixer.hpp"
#include "advanced_platformer/audio/sound_patch.hpp"

TEST_CASE("A mixer overlaps voices and clamps the sum", "[audio][mixer]")
{
    advanced_platformer::SoundMixer mixer;
    const advanced_platformer::SoundBuffer first{{0.75F, -0.75F, 0.2F}, 44100};
    const advanced_platformer::SoundBuffer second{{0.5F, -0.5F}, 44100};
    mixer.play(0, first);
    mixer.play(1, second);
    std::array<float, 4> output{};
    mixer.render(output);
    REQUIRE(output == std::array{1.0F, -1.0F, 0.2F, 0.0F});
    REQUIRE_FALSE(mixer.playing(0));
    REQUIRE_FALSE(mixer.playing(1));
}

TEST_CASE("A voice continues across callback buffers", "[audio][mixer]")
{
    advanced_platformer::SoundMixer mixer;
    const advanced_platformer::SoundBuffer sound{{0.1F, 0.2F, 0.3F}, 44100};
    mixer.play(0, sound);
    std::array<float, 2> output{};
    mixer.render(output);
    REQUIRE(output == std::array{0.1F, 0.2F});
    REQUIRE(mixer.playing(0));
    mixer.render(output);
    REQUIRE(output == std::array{0.3F, 0.0F});
    REQUIRE_FALSE(mixer.playing(0));
}

TEST_CASE("A lower-rate sound is interpolated at the device rate", "[audio][mixer]")
{
    advanced_platformer::SoundMixer mixer;
    const advanced_platformer::SoundBuffer sound{{0.0F, 1.0F, 0.0F}, 22050};
    mixer.play(0, sound);
    std::array<float, 6> output{};
    mixer.render(output);
    REQUIRE(output == std::array{0.0F, 0.5F, 1.0F, 0.5F, 0.0F, 0.0F});
    REQUIRE_FALSE(mixer.playing(0));
}

TEST_CASE("An empty mixer clears the device buffer and invalid slots are ignored", "[audio][mixer]")
{
    advanced_platformer::SoundMixer mixer;
    const advanced_platformer::SoundBuffer sound{{0.5F}, 44100};
    mixer.play(advanced_platformer::SoundVoiceCount, sound);
    mixer.play(0, {});
    std::array<float, 2> output{1.0F, 1.0F};
    mixer.render(output);
    REQUIRE(output == std::array{0.0F, 0.0F});
    REQUIRE_FALSE(mixer.playing(advanced_platformer::SoundVoiceCount));
}
