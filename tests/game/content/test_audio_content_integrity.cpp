#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "advanced_platformer/audio/music.hpp"
#include "advanced_platformer/audio/sound_mixer.hpp"
#include "content/music_song.hpp"

#include "advanced_platformer/world/world.hpp"
#include "content/game_content.hpp"

TEST_CASE(
    "Shipped sound patches render finite samples and presentation hooks resolve their effects",
    "[audio][content][lua]")
{
    auto content = advanced_platformer::loadGameContent("assets");
    for (const auto& [name, sound] : content.gameCatalogs.sounds)
    {
        INFO(name);
        REQUIRE_FALSE(sound->samples.empty());
        REQUIRE(
            std::ranges::all_of(sound->samples, [](float value) { return std::isfinite(value); }));
    }
    for (auto kind :
         {advanced_platformer::WorldEventKind::Shot,
          advanced_platformer::WorldEventKind::Landing,
          advanced_platformer::WorldEventKind::Knockback})
    {
        for (bool player : {true, false})
        {
            const auto effects = content.presentation.onEvent({{}, {}, kind}, player);
            if (effects.sound.has_value())
            {
                REQUIRE(content.gameCatalogs.sounds.contains(effects.sound->name));
            }
            REQUIRE(content.presentation.diagnostics().empty());
        }
    }
}

TEST_CASE(
    "Shipped music songs render finite tracks with matching loop lengths",
    "[audio][content][music]")
{
    for (const auto& entry : std::filesystem::directory_iterator("assets/music"))
    {
        if (entry.path().extension() != ".json")
        {
            continue;
        }
        INFO(entry.path().string());
        const auto parts =
            advanced_platformer::renderMusicParts(advanced_platformer::loadMusicSong(entry.path()));
        const auto frames = parts.tracks.begin()->second.samples.size();
        REQUIRE(frames > 0);
        for (const auto& [name, sound] : parts.tracks)
        {
            INFO(name);
            REQUIRE(sound.samples.size() == frames);
            REQUIRE(sound.sampleRate == advanced_platformer::SoundOutputRate);
            REQUIRE(
                std::ranges::all_of(
                    sound.samples, [](float sample) { return std::isfinite(sample); }));
        }
    }
}
