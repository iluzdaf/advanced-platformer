#include "game/level_requests.hpp"

#include <exception>
#include <format>

#include "debug/console_log.hpp"
#include "game/game.hpp"
#include "game/play_control.hpp"

namespace advanced_platformer
{
    namespace
    {
        void restartGame(Game& game, PlayControl& play, ConsoleLog& console)
        {
            try
            {
                if (!game.restart())
                {
                    console.write(ConsoleLevel::Info, "Only a completed game restarts");
                    return;
                }

                play.restart();
            }
            catch (const std::exception& error)
            {
                console.write(
                    ConsoleLevel::Error,
                    std::format("Could not restart the game: {}", error.what()));
            }
        }

        void restartLevel(Game& game, PlayControl& play, ConsoleLog& console)
        {
            try
            {
                if (!game.restartLevel())
                {
                    console.write(ConsoleLevel::Info, "A completed game has no level to restart");
                    return;
                }

                play.interrupt();
            }
            catch (const std::exception& error)
            {
                console.write(
                    ConsoleLevel::Error,
                    std::format("Could not restart the level: {}", error.what()));
            }
        }

        void rerollLevel(Game& game, PlayControl& play, ConsoleLog& console)
        {
            try
            {
                if (!game.rerollLevel())
                {
                    console.write(ConsoleLevel::Info, "A completed game has no level to reroll");
                    return;
                }

                play.interrupt();
                console.write(
                    ConsoleLevel::Info,
                    std::format(
                        "Generated level {} from seed {}", game.levelNumber(), game.levelSeed()));
            }
            catch (const std::exception& error)
            {
                console.write(
                    ConsoleLevel::Error,
                    std::format("Could not reroll the level: {}", error.what()));
            }
        }
    }

    void applyLevelRequests(
        LevelRequests& requests,
        Game& game,
        PlayControl& play,
        ConsoleLog& console)
    {
        if (requests.restartGame)
        {
            restartGame(game, play, console);
            requests.restartGame = false;
        }
        if (requests.restartLevel)
        {
            restartLevel(game, play, console);
            requests.restartLevel = false;
        }
        if (requests.rerollLevel)
        {
            rerollLevel(game, play, console);
            requests.rerollLevel = false;
        }
    }
}
