#include "launch_options.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include "playtest.hpp"

namespace advanced_platformer
{
    namespace
    {
        constexpr std::string_view Usage =
            "the arguments are [--seed <run seed>] [--bot <directory>] [--level <level>] "
            "[--seconds <seconds per level>]";

        template <class T> T parsedNumber(std::string_view text, std::string_view option)
        {
            T value{};
            const auto [end, error] =
                std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size())
            {
                throw std::invalid_argument(std::string(option) + " needs a number");
            }
            return value;
        }

        std::string_view optionValue(
            std::span<char* const> arguments,
            std::size_t& index,
            std::string_view option)
        {
            if (index + 1 == arguments.size())
            {
                throw std::invalid_argument(std::string(option) + " needs a value");
            }
            return arguments[++index];
        }
    }

    LaunchOptions parseLaunchOptions(std::span<char* const> arguments)
    {
        LaunchOptions options;
        std::optional<float> seconds;
        for (std::size_t index = 1; index < arguments.size(); ++index)
        {
            const std::string_view option = arguments[index];
            if (option == "--seed")
            {
                options.runSeed =
                    parsedNumber<std::uint32_t>(optionValue(arguments, index, option), option);
            }
            else if (option == "--bot")
            {
                options.botDirectory = optionValue(arguments, index, option);
            }
            else if (option == "--level")
            {
                options.level = parsedNumber<int>(optionValue(arguments, index, option), option);
            }
            else if (option == "--seconds")
            {
                seconds = parsedNumber<float>(optionValue(arguments, index, option), option);
            }
            else
            {
                throw std::invalid_argument(std::string(Usage));
            }
        }
        if (options.level.has_value() && *options.level < 1)
        {
            throw std::invalid_argument("--level must be 1 or more");
        }
        if (seconds.has_value() && !(*seconds > 0.0F))
        {
            throw std::invalid_argument("--seconds must be positive");
        }
        if (options.level.has_value() && !options.botDirectory.has_value())
        {
            throw std::invalid_argument("--level needs --bot, which plays the levels before it");
        }
        if (seconds.has_value() && !options.level.has_value())
        {
            throw std::invalid_argument("--seconds needs --level");
        }
        options.secondsPerLevel = seconds.value_or(DefaultPlaytestSecondsPerLevel);
        return options;
    }
}
