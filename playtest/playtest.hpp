#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "advanced_platformer/math/coordinates.hpp"

namespace advanced_platformer
{
    class Game;
    struct GameContent;

    enum class PlaytestOutcome : std::uint8_t
    {
        Exit,
        Defeated,
        Stuck,
        Timeout
    };

    struct NpcNotice
    {
        std::string npc;
        float cells = 0.0F;
    };

    struct PacingSample
    {
        float seconds = 0.0F;
        int health = 0;
        int damage = 0;
        int npcsNear = 0;
        int npcsTargeting = 0;
        std::vector<NpcNotice> noticed;
        std::string piece;
    };

    struct LevelPlaytest
    {
        std::uint32_t runSeed = 0;
        int level = 0;
        std::uint32_t levelSeed = 0;
        PlaytestOutcome outcome = PlaytestOutcome::Timeout;
        float seconds = 0.0F;
        std::map<std::string, int> damageByNearestNpc;
        int healthLeft = 0;
        Cell endCell;
        int pickupsPlaced = 0;
        int pickupsCollected = 0;
        std::vector<std::string> scriptErrors;
        std::vector<PacingSample> pacing;
    };

    GameContent playtestContent(GameContent content, const std::filesystem::path& botDirectory);
    std::vector<LevelPlaytest> playtestRun(
        Game& game,
        int levels,
        float secondsPerLevel,
        float stepSeconds);
    std::string formatLevelPlaytest(const LevelPlaytest& level);
}
