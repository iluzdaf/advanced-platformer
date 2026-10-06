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
        void restartCompletedGame(Game& game, PlayControl& play)
        {
            if (!game.complete())
            {
                return;
            }

            game.restart();
            play.restart();
        }

        void restartLevel(Game& game, PlayControl& play, ConsoleLog& console)
        {
            try
            {
                game.restartLevel();
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
            restartCompletedGame(game, play);
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
