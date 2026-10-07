#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "content/game_content.hpp"
#include "game/game.hpp"
#include "playtest.hpp"
#include "advanced_platformer/timing/fixed_step.hpp"

namespace
{
#ifdef ADVANCED_PLATFORMER_SOURCE_ASSETS
    constexpr const char* AssetDirectory = ADVANCED_PLATFORMER_SOURCE_ASSETS;
#else
    constexpr const char* AssetDirectory = "assets";
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

    int runPlaytest(std::span<char*> arguments)
    {
        if (arguments.size() < 3 || arguments.size() > 4)
        {
            std::cerr << "Usage: advanced_platformer_playtest <run seed> <levels> "
                         "[seconds per level]\n";
            return EXIT_FAILURE;
        }
        const auto runSeed = parsedArgument<std::uint32_t>(arguments[1], "The run seed");
        const int levels = parsedArgument<int>(arguments[2], "The level count");
        const float secondsPerLevel = arguments.size() == 4
                                          ? parsedArgument<float>(arguments[3], "The seconds")
                                          : DefaultSecondsPerLevel;
        if (levels < 1 || !(secondsPerLevel > 0.0F))
        {
            throw std::invalid_argument("The level count and seconds must be positive");
        }

        constexpr auto StepSeconds = static_cast<float>(advanced_platformer::FixedDeltaSeconds);
        advanced_platformer::GameContent content =
            advanced_platformer::loadGameContent(AssetDirectory);
        advanced_platformer::Game game{
            0,
            std::move(content.gameCatalogs),
            std::move(content.npcScripts),
            std::move(content.presentation),
            StepSeconds,
            runSeed};
        for (const advanced_platformer::LevelPlaytest& level :
             advanced_platformer::playtestRun(game, levels, secondsPerLevel, StepSeconds))
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
