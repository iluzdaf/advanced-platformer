#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

#include "launch_options.hpp"
#include "playtest.hpp"

namespace
{
    advanced_platformer::LaunchOptions parsed(std::initializer_list<const char*> options)
    {
        std::vector<std::string> text{"advanced_platformer"};
        text.insert(text.end(), options.begin(), options.end());
        std::vector<char*> arguments;
        arguments.reserve(text.size());
        for (std::string& argument : text)
        {
            arguments.push_back(argument.data());
        }
        return advanced_platformer::parseLaunchOptions(arguments);
    }
}

TEST_CASE("No launch options plays a random run as the player", "[app][launch-options]")
{
    const advanced_platformer::LaunchOptions options = parsed({});

    CHECK_FALSE(options.runSeed.has_value());
    CHECK_FALSE(options.botDirectory.has_value());
    CHECK_FALSE(options.level.has_value());
}

TEST_CASE(
    "Launch options name the run seed, the bot and the level to watch",
    "[app][launch-options]")
{
    const advanced_platformer::LaunchOptions options =
        parsed({"--seed", "24", "--bot", "playtest/assets", "--level", "8", "--seconds", "240"});

    CHECK(options.runSeed == 24U);
    CHECK(options.botDirectory == std::filesystem::path("playtest/assets"));
    CHECK(options.level == 8);
    CHECK(options.secondsPerLevel == 240.0F);
}

TEST_CASE("A watched level gives earlier levels the runner's time limit", "[app][launch-options]")
{
    const advanced_platformer::LaunchOptions options =
        parsed({"--bot", "playtest/assets", "--level", "2"});

    CHECK(options.secondsPerLevel == advanced_platformer::DefaultPlaytestSecondsPerLevel);
}

TEST_CASE("A seed or a bot can be given alone", "[app][launch-options]")
{
    CHECK(parsed({"--seed", "3"}).runSeed == 3U);
    CHECK(parsed({"--bot", "kept/bot"}).botDirectory == std::filesystem::path("kept/bot"));
}

TEST_CASE("A level needs a bot to play the levels before it", "[app][launch-options]")
{
    REQUIRE_THROWS_WITH(
        parsed({"--level", "2"}), "--level needs --bot, which plays the levels before it");
}

TEST_CASE("Seconds per level need a level", "[app][launch-options]")
{
    REQUIRE_THROWS_WITH(
        parsed({"--bot", "playtest/assets", "--seconds", "240"}), "--seconds needs --level");
}

TEST_CASE("Launch options refuse values out of range", "[app][launch-options]")
{
    REQUIRE_THROWS_WITH(
        parsed({"--bot", "playtest/assets", "--level", "0"}), "--level must be 1 or more");
    REQUIRE_THROWS_WITH(
        parsed({"--bot", "playtest/assets", "--level", "2", "--seconds", "0"}),
        "--seconds must be positive");
}

TEST_CASE("Launch options refuse a value that is not a number", "[app][launch-options]")
{
    REQUIRE_THROWS_WITH(parsed({"--seed", "first"}), "--seed needs a number");
    REQUIRE_THROWS_WITH(parsed({"--seed", "-1"}), "--seed needs a number");
}

TEST_CASE("Launch options refuse a missing value", "[app][launch-options]")
{
    REQUIRE_THROWS_WITH(parsed({"--bot"}), "--bot needs a value");
}

TEST_CASE("Launch options refuse an unknown option", "[app][launch-options]")
{
    REQUIRE_THROWS_WITH(
        parsed({"--fast"}),
        "the arguments are [--seed <run seed>] [--bot <directory>] [--level <level>] "
        "[--seconds <seconds per level>]");
}
