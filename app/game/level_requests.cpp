#include "game/level_requests.hpp"

#include <exception>
#include <format>
#include <functional>
#include <utility>

#include "debug/console_log.hpp"
#include "game/game.hpp"
#include "game/play_control.hpp"

namespace advanced_platformer
{
    namespace
    {
        void attempt(
            Game& game,
            ConsoleLog& console,
            bool (Game::*change)(),
            const char* action,
            const char* refusal,
            const std::function<void()>& follow)
        {
            try
            {
                if (!(game.*change)())
                {
                    console.write(ConsoleLevel::Info, refusal);
                    return;
                }

                follow();
            }
            catch (const std::exception& error)
            {
                console.write(
                    ConsoleLevel::Error, std::format("Could not {}: {}", action, error.what()));
            }
        }
    }

    void applyLevelRequests(
        LevelRequests& requests,
        Game& game,
        PlayControl& play,
        ConsoleLog& console)
    {
        if (std::exchange(requests.restartGame, false))
        {
            attempt(
                game,
                console,
                &Game::restart,
                "restart the game",
                "Only a completed game restarts",
                [&play] { play.restart(); });
        }
        if (std::exchange(requests.restartLevel, false))
        {
            attempt(
                game,
                console,
                &Game::restartLevel,
                "restart the level",
                "A completed game has no level to restart",
                [&play] { play.interrupt(); });
        }
        if (std::exchange(requests.rerollLevel, false))
        {
            attempt(
                game,
                console,
                &Game::rerollLevel,
                "reroll the level",
                "A completed game has no level to reroll",
                [&]
                {
                    play.interrupt();
                    console.write(
                        ConsoleLevel::Info,
                        std::format(
                            "Generated level {} from seed {}",
                            game.levelNumber(),
                            game.levelSeed()));
                });
        }
    }
}
