#include "playtest.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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

#include "content/actor_catalog.hpp"
#include "content/actor_definition.hpp"
#include "content/game_content.hpp"
#include "content/machine_catalog.hpp"
#include "content/npc_script_catalog.hpp"
#include "game.hpp"
#include "level/level_composition.hpp"
#include "lua_script_diagnostic.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/level/level_generator.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/npc/npc.hpp"
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
        std::map<std::string, int> damageByNearestNpc;
        int healthLeft = 0;
        std::array<int, 2> endCell{};
        int pickupsPlaced = 0;
        int pickupsCollected = 0;
        std::vector<std::string> scriptErrors;
        std::vector<PacingSample> pacing;
    };

    namespace
    {
        constexpr float BotNoticeDistance = 160.0F;
        constexpr float BotTargetMemorySeconds = 1.0F;
        constexpr const char* BotName = "bot";
    }

    GameContent playtestContent(GameContent content, const std::filesystem::path& botDirectory)
    {
        const MachineCatalog botMachines = loadMachineCatalog(botDirectory / "machines.json");
        for (const auto& [name, machine] : botMachines)
        {
            content.gameCatalogs.machines[name] = machine;
        }
        loadNpcActivityScripts(content.npcScripts, botMachines, botDirectory);

        ActorCatalog& actors = content.gameCatalogs.actors;
        ActorDefinition bot = actorDefinition(actors, actors.player);
        bot.senses = NpcSenses{
            .noticeDistance = BotNoticeDistance, .targetMemoryDuration = BotTargetMemorySeconds};
        bot.machine = BotName;
        validateActorDefinition(
            bot, content.gameCatalogs.animations, content.gameCatalogs.machines);
        actors.definitions[BotName] = std::move(bot);
        actors.player = BotName;
        return content;
    }

    namespace
    {
        struct PlaytestWatch
        {
            int lastHealth = 0;
            float bestExitDistance = std::numeric_limits<float>::max();
            float secondsSinceProgress = 0.0F;
            int steps = 0;
            int damageSinceSample = 0;
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

        int npcsNear(const GameLevel& level, const Actor& player)
        {
            const glm::vec2 center = centerOf(player.body.bounds);
            return static_cast<int>(std::ranges::count_if(
                level.world.actors(),
                [&](const Actor& actor)
                {
                    return actor.id != player.id && actor.life == LifeState::Alive &&
                           level.actorDefinitionNames.contains(actor.id.value) &&
                           glm::distance(center, centerOf(actor.body.bounds)) <= BotNoticeDistance;
                }));
        }

        std::string pieceAt(const GameLevel& level, Cell cell)
        {
            for (const GeneratedRoom& room : level.rooms)
            {
                if (contains(room.size, {cell.x - room.origin.x, cell.y - room.origin.y}))
                {
                    return room.piece;
                }
            }
            return "";
        }

        PacingSample pacingSample(const GameLevel& level, const Actor& player, float seconds)
        {
            return {
                .seconds = seconds,
                .health = healthOf(player),
                .npcsNear = npcsNear(level, player),
                .piece =
                    pieceAt(level, cellAtFeet(level.map.tileSize(), feetOf(player.body.bounds)))};
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
        void observeStep(
            PlaytestWatch& watch,
            const GameLevel& level,
            float stepSeconds,
            LevelPlaytest& result)
        {
            const Actor& player = playerOf(level);
            const int health = healthOf(player);
            if (health < watch.lastHealth)
            {
                result.damageByNearestNpc[nearestNpcName(level, player)] +=
                    watch.lastHealth - health;
                watch.damageSinceSample += watch.lastHealth - health;
            }
            watch.lastHealth = health;

            const glm::vec2 feet = feetOf(player.body.bounds);
            const float exitDistance = glm::distance(feet, exitFeet(level));
            if (exitDistance < watch.bestExitDistance - static_cast<float>(level.map.tileSize()))
            {
                watch.bestExitDistance = exitDistance;
                watch.secondsSinceProgress = 0.0F;
            }
            else
            {
                watch.secondsSinceProgress += stepSeconds;
            }

            result.healthLeft = health;
            result.endCell = cellAtFeet(level.map.tileSize(), feet);
            result.pickupsCollected = result.pickupsPlaced - placedPickupsLeft(level);
        }
    }

    namespace
    {
        float watchedSeconds(const PlaytestWatch& watch, float stepSeconds)
        {
            const double seconds = static_cast<double>(watch.steps) * stepSeconds;
            return static_cast<float>(std::round(seconds * 1000.0) / 1000.0);
        }

        void recordPacing(
            PlaytestWatch& watch,
            const GameLevel& level,
            float stepSeconds,
            LevelPlaytest& result)
        {
            PacingSample sample =
                pacingSample(level, playerOf(level), watchedSeconds(watch, stepSeconds));
            sample.damage = std::exchange(watch.damageSinceSample, 0);
            result.pacing.push_back(std::move(sample));
        }
    }

    namespace
    {
        constexpr float StuckSeconds = 15.0F;
        constexpr float PacingSeconds = 0.5F;

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

            const int stepsPerSample =
                std::max(1, static_cast<int>(std::lround(PacingSeconds / stepSeconds)));
            PlaytestWatch watch;
            watch.lastHealth = healthOf(playerOf(level));
            observeStep(watch, level, 0.0F, result);
            recordPacing(watch, level, stepSeconds, result);

            while (result.seconds < secondsPerLevel)
            {
                const Actor& player = playerOf(level);
                const std::string nearest = nearestNpcName(level, player);
                const int healthBefore = healthOf(player);
                PacingSample last = pacingSample(level, player, 0.0F);

                game.update({}, stepSeconds);
                result.seconds += stepSeconds;
                ++watch.steps;
                recordScriptErrors(game, result);

                if (game.runSeed() != runSeed)
                {
                    result.outcome = PlaytestOutcome::Defeated;
                    result.damageByNearestNpc[nearest] += healthBefore;
                    result.healthLeft = 0;
                    last.seconds = watchedSeconds(watch, stepSeconds);
                    last.health = 0;
                    last.damage = watch.damageSinceSample + healthBefore;
                    result.pacing.push_back(std::move(last));
                    return result;
                }
                if (game.levelNumber() != levelNumber)
                {
                    result.outcome = PlaytestOutcome::Exit;
                    last.seconds = watchedSeconds(watch, stepSeconds);
                    last.damage = watch.damageSinceSample;
                    result.pacing.push_back(std::move(last));
                    return result;
                }
                observeStep(watch, level, stepSeconds, result);
                if (watch.secondsSinceProgress >= StuckSeconds)
                {
                    result.outcome = PlaytestOutcome::Stuck;
                    recordPacing(watch, level, stepSeconds, result);
                    return result;
                }
                if (watch.steps % stepsPerSample == 0)
                {
                    recordPacing(watch, level, stepSeconds, result);
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
            .damageByNearestNpc = level.damageByNearestNpc,
            .healthLeft = level.healthLeft,
            .endCell = {level.endCell.x, level.endCell.y},
            .pickupsPlaced = level.pickupsPlaced,
            .pickupsCollected = level.pickupsCollected,
            .scriptErrors = level.scriptErrors,
            .pacing = level.pacing};
        std::string text;
        if (const auto error = glz::write_json(json, text))
        {
            throw std::runtime_error("The playtest could not write its level report");
        }
        return text;
    }
}
