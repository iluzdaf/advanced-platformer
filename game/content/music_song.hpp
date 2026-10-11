#pragma once

#include <filesystem>
#include <memory>

#include "advanced_platformer/audio/sound_patch.hpp"
#include <string_view>

#include "advanced_platformer/audio/music.hpp"

namespace advanced_platformer
{
    MusicSong parseMusicSong(std::string_view text, std::string_view sourceName);
    std::shared_ptr<const SoundBuffer> loadBackgroundMusic(
        const std::filesystem::path& assetDirectory);
    MusicSong loadMusicSong(const std::filesystem::path& path);
}
