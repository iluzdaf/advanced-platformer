#pragma once

#include <filesystem>

#include "advanced_platformer/audio/sound_patch.hpp"

namespace advanced_platformer
{
    void writeWaveFile(const std::filesystem::path& path, const SoundBuffer& sound);
}
