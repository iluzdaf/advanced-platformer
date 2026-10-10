#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "advanced_platformer/audio/sound_patch.hpp"

namespace advanced_platformer
{
    using SoundCatalog = std::map<std::string, std::shared_ptr<const SoundBuffer>, std::less<>>;

    SoundCatalog parseSoundCatalog(std::string_view text, std::string_view sourceName);
    SoundCatalog loadSoundCatalog(const std::filesystem::path& path);
}
