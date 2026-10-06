#pragma once

namespace advanced_platformer
{
    class ConsoleLog;
    class Game;
    class PlayControl;

    struct LevelRequests
    {
        bool restartGame = false;
        bool restartLevel = false;
        bool rerollLevel = false;
    };

    void applyLevelRequests(
        LevelRequests& requests,
        Game& game,
        PlayControl& play,
        ConsoleLog& console);
}
