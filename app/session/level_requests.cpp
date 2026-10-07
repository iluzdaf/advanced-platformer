#include "session/level_requests.hpp"

#include <exception>
#include <format>

#include "diagnostics/console_log.hpp"
#include "game.hpp"
#include "session/play_control.hpp"

namespace advanced_platformer
{
    namespace
    {
        void restartLevel(Game& game, PlayControl& play, ConsoleLog& console)
        {
            try
            {
                game.changeLevel(LevelChange::Restart);
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
                game.changeLevel(LevelChange::Reroll);
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
