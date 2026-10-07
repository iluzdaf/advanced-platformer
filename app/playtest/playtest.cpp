#include "playtest.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glaze/glaze.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "game/game.hpp"
#include "game/level_composition.hpp"
#include "lua_script_diagnostic.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/navigation/actor_navigation.hpp"
#include "advanced_platformer/navigation/navigation_fill.hpp"
#include "advanced_platformer/navigation/navigation_path.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/world/level_exit.hpp"
#include "advanced_platformer/world/pickup.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"

namespace advanced_platformer
{
    struct LevelPlaytestJson
    {
        std::uint32_t runSeed = 0;
        int level = 0;
        std::uint32_t levelSeed = 0;
        std::string outcome;
        float seconds = 0.0F;
        int replans = 0;
        int unreachablePlans = 0;
        std::map<std::string, int> damageByNearestNpc;
        int healthLeft = 0;
        std::array<int, 2> endCell{};
        int pickupsPlaced = 0;
        int pickupsCollected = 0;
        std::vector<std::string> scriptErrors;
    };

    namespace
    {
        struct PlaytestBot
        {
            PlatformerConnectionCache cache;
            PathFollower follower;
            std::size_t breaksWhenPlanned = 0;
            int lastHealth = 0;
            glm::vec2 lastFeet{0.0F, 0.0F};
            float stillSeconds = 0.0F;
            float bestExitDistance = std::numeric_limits<float>::max();
            float secondsSinceProgress = 0.0F;
        };

        const Actor& playerOf(const GameLevel& level)
        {
            const Actor* player = level.world.findActor(level.world.playerId());
            if (player == nullptr)
            {
                throw std::logic_error("A playtest needs a player");
            }
            return *player;
        }

        int healthOf(const Actor& player)
        {
            if (!player.health.has_value())
            {
                throw std::logic_error("A playtest needs a player with health");
            }
            return player.health->current;
        }

        glm::vec2 exitFeet(const GameLevel& level)
        {
            const std::optional<LevelExit>& exit = level.world.exit();
            if (!exit.has_value())
            {
                throw std::logic_error("A playtest needs a level with an exit");
            }
            return feetOf(exit->bounds);
        }

        std::string nearestNpcName(const GameLevel& level, const Actor& player)
        {
            const glm::vec2 center = centerOf(player.body.bounds);
            std::string nearest = "unknown";
            float nearestDistance = std::numeric_limits<float>::max();
            for (const Actor& actor : level.world.actors())
            {
                const auto name = level.actorDefinitionNames.find(actor.id.value);
                if (actor.id == player.id || name == level.actorDefinitionNames.end() ||
                    actor.life != LifeState::Alive)
                {
                    continue;
                }
                const float distance = glm::distance(center, centerOf(actor.body.bounds));
                if (distance < nearestDistance)
                {
                    nearestDistance = distance;
                    nearest = name->second;
                }
            }
            return nearest;
        }
    }

    namespace
    {
        constexpr float StillSecondsBeforeReplan = 1.0F;

        bool needsPlan(const PlaytestBot& bot, const TileMap& map)
        {
            return !bot.follower.path.has_value() || pathComplete(bot.follower) ||
                   map.brokenCells().size() != bot.breaksWhenPlanned ||
                   bot.stillSeconds >= StillSecondsBeforeReplan;
        }

        void planToExit(
            PlaytestBot& bot,
            const GameLevel& level,
            const Actor& player,
            float stepSeconds,
            LevelPlaytest& result)
        {
            std::optional<NavigationPathResult> found =
                findActorPath(level.map, player, exitFeet(level), stepSeconds, bot.cache);
            while (found.has_value() && found->status == NavigationPathStatus::Deferred)
            {
                advanceNavigationFill(level.map, bot.cache, 1);
                found = findActorPath(level.map, player, exitFeet(level), stepSeconds, bot.cache);
            }
            if (!found.has_value() || !found->path.has_value())
            {
                return;
            }
            ++result.replans;
            if (found->status == NavigationPathStatus::Unreachable)
            {
                ++result.unreachablePlans;
            }
            bot.breaksWhenPlanned = level.map.brokenCells().size();
            bot.stillSeconds = 0.0F;
            setPath(bot.follower, std::move(*found->path));
        }
    }

    namespace
    {
        InputIntentions botIntentions(
            PlaytestBot& bot,
            const GameLevel& level,
            const Actor& player,
            float stepSeconds,
            LevelPlaytest& result)
        {
            if (needsPlan(bot, level.map))
            {
                clearPath(bot.follower);
                planToExit(bot, level, player, stepSeconds, result);
            }
            if (!bot.follower.path.has_value() || !player.platformerMovement.has_value())
            {
                return {};
            }
            return followPlatformerPath(
                player.body,
                *player.platformerMovement,
                bot.follower,
                stepSeconds,
                player.surfaceClimb.has_value() ? &*player.surfaceClimb : nullptr);
        }
    }

    namespace
    {
        std::string scriptErrorText(const LuaScriptDiagnostic& diagnostic)
        {
            return std::format(
                "{} {} {}: {}",
                diagnostic.script,
                diagnostic.activity,
                diagnostic.hook,
                diagnostic.message);
        }
    }

    namespace
    {
        void recordScriptErrors(Game& game, LevelPlaytest& result)
        {
            for (const LuaScriptDiagnostic& diagnostic : game.takeScriptDiagnostics())
            {
                if (diagnostic.kind != LuaScriptDiagnosticKind::Error)
                {
                    continue;
                }
                std::string text = scriptErrorText(diagnostic);
                if (std::ranges::find(result.scriptErrors, text) == result.scriptErrors.end())
                {
                    result.scriptErrors.push_back(std::move(text));
                }
            }
        }
    }

    namespace
    {
        int placedPickupsLeft(const GameLevel& level)
        {
            return static_cast<int>(std::ranges::count_if(
                level.world.pickups(),
                [](const Pickup& pickup) { return pickup.placement.has_value(); }));
        }
    }

    namespace
    {
        constexpr float StillDistance = 0.5F;

        void observeStep(
            PlaytestBot& bot,
            const GameLevel& level,
            float stepSeconds,
            LevelPlaytest& result)
        {
            const Actor& player = playerOf(level);
            const int health = healthOf(player);
            if (health < bot.lastHealth)
            {
                result.damageByNearestNpc[nearestNpcName(level, player)] += bot.lastHealth - health;
                clearPath(bot.follower);
            }
            bot.lastHealth = health;

            const glm::vec2 feet = feetOf(player.body.bounds);
            bot.stillSeconds = glm::distance(feet, bot.lastFeet) < StillDistance
                                   ? bot.stillSeconds + stepSeconds
                                   : 0.0F;
            bot.lastFeet = feet;

            const float exitDistance = glm::distance(feet, exitFeet(level));
            if (exitDistance < bot.bestExitDistance - static_cast<float>(level.map.tileSize()))
            {
                bot.bestExitDistance = exitDistance;
                bot.secondsSinceProgress = 0.0F;
            }
            else
            {
                bot.secondsSinceProgress += stepSeconds;
            }

            result.healthLeft = health;
            result.endCell = cellAtFeet(level.map.tileSize(), feet);
            result.pickupsCollected = result.pickupsPlaced - placedPickupsLeft(level);
        }
    }

    namespace
    {
        constexpr float StuckSeconds = 15.0F;

        LevelPlaytest playtestLevel(Game& game, float secondsPerLevel, float stepSeconds)
        {
            const GameLevel& level = game.currentLevel();
            const std::uint32_t runSeed = game.runSeed();
            const int levelNumber = game.levelNumber();
            LevelPlaytest result{
                .runSeed = runSeed,
                .level = levelNumber,
                .levelSeed = game.levelSeed(),
                .pickupsPlaced = static_cast<int>(level.pickupPlacementIds.size())};

            PlaytestBot bot;
            const Actor& start = playerOf(level);
            bot.lastHealth = healthOf(start);
            bot.lastFeet = feetOf(start.body.bounds);
            observeStep(bot, level, 0.0F, result);

            while (result.seconds < secondsPerLevel)
            {
                const Actor& player = playerOf(level);
                const InputIntentions intentions =
                    botIntentions(bot, level, player, stepSeconds, result);
                const std::string nearest = nearestNpcName(level, player);
                const int healthBefore = healthOf(player);

                game.update(intentions, stepSeconds);
                result.seconds += stepSeconds;
                recordScriptErrors(game, result);

                if (game.runSeed() != runSeed)
                {
                    result.outcome = PlaytestOutcome::Defeated;
                    result.damageByNearestNpc[nearest] += healthBefore;
                    result.healthLeft = 0;
                    return result;
                }
                if (game.levelNumber() != levelNumber)
                {
                    result.outcome = PlaytestOutcome::Exit;
                    return result;
                }
                observeStep(bot, level, stepSeconds, result);
                if (bot.secondsSinceProgress >= StuckSeconds)
                {
                    result.outcome = PlaytestOutcome::Stuck;
                    return result;
                }
            }
            result.outcome = PlaytestOutcome::Timeout;
            return result;
        }
    }

    std::vector<LevelPlaytest> playtestRun(
        Game& game,
        int levels,
        float secondsPerLevel,
        float stepSeconds)
    {
        std::vector<LevelPlaytest> results;
        for (int played = 0; played < levels; ++played)
        {
            results.push_back(playtestLevel(game, secondsPerLevel, stepSeconds));
            const PlaytestOutcome outcome = results.back().outcome;
            if (outcome == PlaytestOutcome::Defeated)
            {
                break;
            }
            if (outcome != PlaytestOutcome::Exit)
            {
                game.changeLevel(LevelChange::NextLevel);
            }
        }
        return results;
    }

    namespace
    {
        std::string_view outcomeName(PlaytestOutcome outcome)
        {
            switch (outcome)
            {
            case PlaytestOutcome::Exit:
                return "exit";
            case PlaytestOutcome::Defeated:
                return "defeated";
            case PlaytestOutcome::Stuck:
                return "stuck";
            case PlaytestOutcome::Timeout:
                return "timeout";
            }
            return "timeout";
        }
    }

    std::string formatLevelPlaytest(const LevelPlaytest& level)
    {
        const LevelPlaytestJson json{
            .runSeed = level.runSeed,
            .level = level.level,
            .levelSeed = level.levelSeed,
            .outcome = std::string(outcomeName(level.outcome)),
            .seconds = level.seconds,
            .replans = level.replans,
            .unreachablePlans = level.unreachablePlans,
            .damageByNearestNpc = level.damageByNearestNpc,
            .healthLeft = level.healthLeft,
            .endCell = {level.endCell.x, level.endCell.y},
            .pickupsPlaced = level.pickupsPlaced,
            .pickupsCollected = level.pickupsCollected,
            .scriptErrors = level.scriptErrors};
        std::string text;
        if (const auto error = glz::write_json(json, text))
        {
            throw std::runtime_error("The playtest could not write its level report");
        }
        return text;
    }
}
