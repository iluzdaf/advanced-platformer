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
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
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
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/level/level_generator.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/world/level_exit.hpp"
#include "advanced_platformer/world/pickup.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"

template <> struct glz::meta<advanced_platformer::Cell>
{
    using T = advanced_platformer::Cell;
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr auto value = glz::array(&T::x, &T::y);
};

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
        std::vector<std::string> map;
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
            std::unordered_set<Cell, CellHash> visitedCells;
            float secondsSinceProgress = 0.0F;
            int steps = 0;
            int damageSinceSample = 0;
            std::vector<ActorId> targeting;
            std::vector<NpcNotice> noticedSinceSample;
            std::map<std::size_t, std::string> pickupsLeft;
            std::vector<std::string> collectedSinceSample;
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

        std::string nearestNpcName(const GameLevel& level, const Actor& player)
        {
            const glm::vec2 center = centerOf(player.body.bounds);
            std::string nearest = "unknown";
            float nearestDistance = std::numeric_limits<float>::max();
            for (const Actor& actor : level.world.actors())
            {
                if (actor.id == player.id || actor.definitionName.empty() ||
                    actor.life != LifeState::Alive)
                {
                    continue;
                }
                const float distance = glm::distance(center, centerOf(actor.body.bounds));
                if (distance < nearestDistance)
                {
                    nearestDistance = distance;
                    nearest = actor.definitionName;
                }
            }
            return nearest;
        }

        bool targetsPlayer(const Actor& actor, const Actor& player)
        {
            return actor.id != player.id && actor.life == LifeState::Alive &&
                   !actor.definitionName.empty() && actor.brain.has_value() &&
                   actor.brain->target == player.id;
        }

        int npcsTargeting(const GameLevel& level, const Actor& player)
        {
            return static_cast<int>(std::ranges::count_if(
                level.world.actors(),
                [&](const Actor& actor) { return targetsPlayer(actor, player); }));
        }

        void observeNotices(PlaytestWatch& watch, const GameLevel& level, const Actor& player)
        {
            std::vector<ActorId> targeting;
            for (const Actor& actor : level.world.actors())
            {
                if (!targetsPlayer(actor, player))
                {
                    continue;
                }
                targeting.push_back(actor.id);
                if (std::ranges::find(watch.targeting, actor.id) == watch.targeting.end())
                {
                    const float cells =
                        glm::distance(centerOf(player.body.bounds), centerOf(actor.body.bounds)) /
                        static_cast<float>(level.map.tileSize());
                    watch.noticedSinceSample.push_back(
                        {.npc = actor.definitionName, .cells = std::round(cells * 10.0F) / 10.0F});
                }
            }
            watch.targeting = std::move(targeting);
        }

        bool nearPlayer(const Actor& actor, const Actor& player)
        {
            return actor.id != player.id && actor.life == LifeState::Alive &&
                   !actor.definitionName.empty() &&
                   glm::distance(centerOf(player.body.bounds), centerOf(actor.body.bounds)) <=
                       BotNoticeDistance;
        }

        float tenthsOfCells(float pixels, int tileSize)
        {
            return std::round(pixels / static_cast<float>(tileSize) * 10.0F) / 10.0F;
        }

        std::vector<NpcNearby> npcsNearby(const GameLevel& level, const Actor& player)
        {
            const glm::vec2 center = centerOf(player.body.bounds);
            std::vector<NpcNearby> nearby;
            for (const Actor& actor : level.world.actors())
            {
                if (!nearPlayer(actor, player))
                {
                    continue;
                }
                const glm::vec2 offset = centerOf(actor.body.bounds) - center;
                nearby.push_back(
                    {.npc = actor.definitionName,
                     .state = actor.machine.has_value() ? activeNpcMachineState(*actor.machine).name
                                                        : "",
                     .right = tenthsOfCells(offset.x, level.map.tileSize()),
                     .above = tenthsOfCells(-offset.y, level.map.tileSize())});
            }
            return nearby;
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
            const Cell cell = cellAtFeet(level.map.tileSize(), feetOf(player.body.bounds));
            std::vector<NpcNearby> npcs = npcsNearby(level, player);
            const int near = static_cast<int>(npcs.size());
            return {
                .seconds = seconds,
                .health = healthOf(player),
                .npcsNear = near,
                .npcsTargeting = npcsTargeting(level, player),
                .npcs = std::move(npcs),
                .piece = pieceAt(level, cell),
                .cell = cell};
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
        std::map<std::size_t, std::string> placedPickupsLeft(const GameLevel& level)
        {
            std::map<std::size_t, std::string> left;
            for (const Pickup& pickup : level.world.pickups())
            {
                if (pickup.placement.has_value())
                {
                    left.emplace(
                        *pickup.placement, level.world.itemDefinition(pickup.stack.item).name);
                }
            }
            return left;
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
            observeNotices(watch, level, player);

            const glm::vec2 feet = feetOf(player.body.bounds);
            const Cell cell = cellAtFeet(level.map.tileSize(), feet);
            if (watch.visitedCells.insert(cell).second)
            {
                watch.secondsSinceProgress = 0.0F;
            }
            else
            {
                watch.secondsSinceProgress += stepSeconds;
            }

            result.healthLeft = health;
            result.endCell = cell;
            std::map<std::size_t, std::string> pickupsLeft = placedPickupsLeft(level);
            for (const auto& [placement, item] : watch.pickupsLeft)
            {
                if (!pickupsLeft.contains(placement))
                {
                    watch.collectedSinceSample.push_back(item);
                }
            }
            watch.pickupsLeft = std::move(pickupsLeft);
            result.pickupsCollected =
                result.pickupsPlaced - static_cast<int>(watch.pickupsLeft.size());
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
            sample.noticed = std::exchange(watch.noticedSinceSample, {});
            sample.collected = std::exchange(watch.collectedSinceSample, {});
            result.pacing.push_back(std::move(sample));
        }
    }

    namespace
    {
        constexpr float StuckSeconds = 15.0F;
        constexpr float PacingSeconds = 0.5F;

        char tileLetter(const TileDefinition& tile)
        {
            if (tile.breaksIntoTileId.has_value())
            {
                return 'X';
            }
            if (tile.blocksMovement)
            {
                return '#';
            }
            return tile.blocksSight ? 'G' : '.';
        }

        std::vector<std::string> levelMap(const TileMap& map)
        {
            std::vector<std::string> rows(
                static_cast<std::size_t>(map.height()),
                std::string(static_cast<std::size_t>(map.width()), '.'));
            for (int y = 0; y < map.height(); ++y)
            {
                for (int x = 0; x < map.width(); ++x)
                {
                    rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                        tileLetter(map.definitionAt({x, y}));
                }
            }
            return rows;
        }

        LevelPlaytest playtestLevel(Game& game, float secondsPerLevel, float stepSeconds)
        {
            const GameLevel& level = game.currentLevel();
            const std::uint32_t runSeed = game.runSeed();
            const int levelNumber = game.levelNumber();
            LevelPlaytest result{
                .runSeed = runSeed,
                .level = levelNumber,
                .levelSeed = game.levelSeed(),
                .pickupsPlaced = static_cast<int>(level.pickupPlacementIds.size()),
                .map = levelMap(level.map)};

            const int stepsPerSample =
                std::max(1, static_cast<int>(std::lround(PacingSeconds / stepSeconds)));
            PlaytestWatch watch;
            watch.lastHealth = healthOf(playerOf(level));
            watch.pickupsLeft = placedPickupsLeft(level);
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
                    last.noticed = std::move(watch.noticedSinceSample);
                    last.collected = std::move(watch.collectedSinceSample);
                    result.pacing.push_back(std::move(last));
                    return result;
                }
                if (game.levelNumber() != levelNumber)
                {
                    result.outcome = PlaytestOutcome::Exit;
                    last.seconds = watchedSeconds(watch, stepSeconds);
                    last.damage = watch.damageSinceSample;
                    last.noticed = std::move(watch.noticedSinceSample);
                    last.collected = std::move(watch.collectedSinceSample);
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
            .map = level.map,
            .pacing = level.pacing};
        std::string text;
        if (const auto error = glz::write_json(json, text))
        {
            throw std::runtime_error("The playtest could not write its level report");
        }
        return text;
    }
}
