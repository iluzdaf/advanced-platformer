#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

#include "advanced_platformer/audio/sound_patch.hpp"
#include "audio/wave_file.hpp"

TEST_CASE("Music previews export mono float WAV with exact samples", "[audio][music]")
{
    const auto path = std::filesystem::temp_directory_path() / "advanced-platformer-wave-test.wav";
    const advanced_platformer::SoundBuffer sound{{0.0F, 0.5F, -0.5F}, 44100};
    advanced_platformer::writeWaveFile(path, sound);
    std::ifstream input(path, std::ios::binary);
    const std::string bytes{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    std::filesystem::remove(path);
    REQUIRE(bytes.size() == 68);
    REQUIRE(bytes.substr(0, 4) == "RIFF");
    REQUIRE(bytes.substr(8, 8) == "WAVEfmt ");
    REQUIRE(static_cast<unsigned char>(bytes[20]) == 3);
    REQUIRE(static_cast<unsigned char>(bytes[22]) == 1);
    REQUIRE(static_cast<unsigned char>(bytes[34]) == 32);
    REQUIRE(bytes.substr(36, 4) == "fact");
    REQUIRE(bytes.substr(48, 4) == "data");
    REQUIRE(static_cast<unsigned char>(bytes[44]) == 3);
    REQUIRE(static_cast<unsigned char>(bytes[63]) == 0x3f);
    REQUIRE(static_cast<unsigned char>(bytes[67]) == 0xbf);
}

TEST_CASE("Music previews reject invalid buffers and unwritable destinations", "[audio][music]")
{
    REQUIRE_THROWS_AS(advanced_platformer::writeWaveFile("unused.wav", {}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        advanced_platformer::writeWaveFile(
            "unused.wav", {{std::numeric_limits<float>::quiet_NaN()}, 44100}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        advanced_platformer::writeWaveFile("missing-directory/preview.wav", {{0}, 44100}),
        std::runtime_error);
}
