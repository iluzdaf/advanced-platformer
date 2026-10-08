#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "content/game_content.hpp"
#include "game.hpp"
#include "level/level_composition.hpp"
#include "playtest.hpp"
#include "advanced_platformer/timing/fixed_step.hpp"

namespace
{
#ifdef ADVANCED_PLATFORMER_SOURCE_ASSETS
    constexpr const char* AssetDirectory = ADVANCED_PLATFORMER_SOURCE_ASSETS;
#else
    constexpr const char* AssetDirectory = "assets";
#endif
#ifdef ADVANCED_PLATFORMER_SOURCE_PLAYTEST_ASSETS
    constexpr const char* PlaytestAssetDirectory = ADVANCED_PLATFORMER_SOURCE_PLAYTEST_ASSETS;
#else
    constexpr const char* PlaytestAssetDirectory = "playtest/assets";
#endif
    constexpr float DefaultSecondsPerLevel = 120.0F;

    template <class T> T parsedArgument(std::string_view text, std::string_view name)
    {
        T value{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size())
        {
            throw std::invalid_argument(std::string(name) + " must be a number");
        }
        return value;
    }

    struct PlaytestArguments
    {
        std::uint32_t runSeed = 0;
        int levels = 0;
        float secondsPerLevel = DefaultSecondsPerLevel;
        std::filesystem::path botDirectory = PlaytestAssetDirectory;
    };

    PlaytestArguments parsedArguments(std::span<char*> arguments)
    {
        PlaytestArguments parsed;
        std::vector<std::string_view> positional;
        for (std::size_t index = 1; index < arguments.size(); ++index)
        {
            const std::string_view argument = arguments[index];
            if (argument == "--bot")
            {
                if (index + 1 == arguments.size())
                {
                    throw std::invalid_argument("--bot needs a directory");
                }
                parsed.botDirectory = arguments[++index];
            }
            else
            {
                positional.push_back(argument);
            }
        }
        if (positional.size() < 2 || positional.size() > 3)
        {
            throw std::invalid_argument(
                "the arguments are <run seed> <levels> [seconds per level] [--bot <directory>]");
        }
        parsed.runSeed = parsedArgument<std::uint32_t>(positional[0], "The run seed");
        parsed.levels = parsedArgument<int>(positional[1], "The level count");
        if (positional.size() == 3)
        {
            parsed.secondsPerLevel = parsedArgument<float>(positional[2], "The seconds");
        }
        if (parsed.levels < 1 || !(parsed.secondsPerLevel > 0.0F))
        {
            throw std::invalid_argument("The level count and seconds must be positive");
        }
        return parsed;
    }

    int runPlaytest(std::span<char*> arguments)
    {
        const PlaytestArguments parsed = parsedArguments(arguments);
        constexpr auto StepSeconds = static_cast<float>(advanced_platformer::FixedDeltaSeconds);
        advanced_platformer::GameContent content = advanced_platformer::playtestContent(
            advanced_platformer::loadGameContent(AssetDirectory), parsed.botDirectory);
        advanced_platformer::validateRoomPieces(content.gameCatalogs, StepSeconds);
        advanced_platformer::Game game{
            0,
            std::move(content.gameCatalogs),
            std::move(content.npcScripts),
            std::move(content.presentation),
            StepSeconds,
            parsed.runSeed};
        for (const advanced_platformer::LevelPlaytest& level : advanced_platformer::playtestRun(
                 game, parsed.levels, parsed.secondsPerLevel, StepSeconds))
        {
            std::cout << advanced_platformer::formatLevelPlaytest(level) << '\n';
        }
        return EXIT_SUCCESS;
    }
}

int main(int argc, char** argv)
{
    try
    {
        return runPlaytest({argv, static_cast<std::size_t>(argc)});
    }
    catch (const std::exception& error)
    {
        std::cerr << "The playtest stopped: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
