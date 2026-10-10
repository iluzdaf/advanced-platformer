#pragma once

#include <filesystem>
#include <string_view>

#include "advanced_platformer/audio/music.hpp"

namespace advanced_platformer
{
    MusicSong parseMusicSong(std::string_view text, std::string_view sourceName);
    MusicSong loadMusicSong(const std::filesystem::path& path);
}
