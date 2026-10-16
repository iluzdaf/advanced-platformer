#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>

namespace advanced_platformer
{
    struct LaunchOptions
    {
        std::optional<std::uint32_t> runSeed;
        std::optional<std::filesystem::path> botDirectory;
        std::optional<int> level;
        float secondsPerLevel = 0.0F;
    };

    LaunchOptions parseLaunchOptions(std::span<char* const> arguments);
}
