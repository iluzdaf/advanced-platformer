#include "game.hpp"

#include "diagnostics/debug_overlay.hpp"
#include "content/actor_catalog.hpp"
#include "diagnostics/navigation_debug.hpp"
#include "level/level_composition.hpp"
#include "advanced_platformer/level/level_generator.hpp"
#include "content/game_catalogs.hpp"
#include "content/hud_catalog.hpp"
#include "content/game_content.hpp"
#include "level/level_reload.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <cmath>
#include <optional>
#include <variant>
#include <vector>
#include <stdexcept>
#include <utility>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/inventory/inventory.hpp"
#include "advanced_platformer/inventory/item.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/navigation/navigation_fill.hpp"
#include "advanced_platformer/render/camera.hpp"
#include "advanced_platformer/render/presentation.hpp"
#include "advanced_platformer/render/render_scene.hpp"
#include "advanced_platformer/render/sprite.hpp"
#include "lua_npc_scripts.hpp"
#include "advanced_platformer/world/world.hpp"
#include "lua_script_diagnostic.hpp"
#include "lua_presentation_script.hpp"
#include "advanced_platformer/world/level_exit.hpp"
#include "advanced_platformer/world/level_validation.hpp"
#include "advanced_platformer/world/world_requests.hpp"
#include "advanced_platformer/world/world_simulation.hpp"

namespace advanced_platformer
{
    namespace
    {
        GameCatalogs checkedCatalogs(GameCatalogs catalogs, float stepSeconds)
        {
            if (!isFinitePositive(stepSeconds))
            {
                throw std::invalid_argument(
                    "The game's simulation step must be finite and positive");
            }
            validateRoomPieces(catalogs, stepSeconds);
            return catalogs;
        }
    }

    Game::Game(
        int textureId,
        GameCatalogs gameCatalogs,
        LuaNpcScripts npcScripts,
        LuaPresentationScript presentation,
        float stepSeconds,
        std::uint32_t runSeed)
        : gameCatalogs(checkedCatalogs(std::move(gameCatalogs), stepSeconds)),
          npcScripts(std::move(npcScripts)),
          presentation(std::move(presentation)),
          level(composePlayableLevel(
              this->gameCatalogs.pieces,
              1,
              runLevelSeed(runSeed, 1),
              textureId,
              this->gameCatalogs,
              composePlayer(this->gameCatalogs, textureId),
              stepSeconds)),
          atlasTextureId(textureId),
          simulationStepSeconds(stepSeconds),
          currentRunSeed(runSeed)
    {
        prepareLevel();
    }

    void Game::changeLevel(LevelChange change)
    {
        switch (change)
        {
        case LevelChange::NewRun:
            enterLevel(
                1, runLevelSeed(currentRunSeed, 1), composePlayer(gameCatalogs, atlasTextureId));
            return;
        case LevelChange::NextLevel:
            enterLevel(
                level.number + 1, runLevelSeed(currentRunSeed, level.number + 1), carriedPlayer());
            return;
        case LevelChange::Restart:
            enterLevel(level.number, level.seed, carriedPlayer());
            return;
        case LevelChange::Reroll:
            enterLevel(level.number, level.seed + 1U, carriedPlayer());
            return;
        }
    }

    Actor Game::carriedPlayer() const
    {
        Actor next = composePlayer(gameCatalogs, atlasTextureId);
        if (const Actor* live = level.world.findActor(level.world.playerId()))
        {
            next.health = live->health;
            next.inventory = live->inventory;
        }
        return next;
    }

    void Game::enterLevel(int levelNumber, std::uint32_t seed, const Actor& player)
    {
        GameLevel next = composePlayableLevel(
            gameCatalogs.pieces,
            levelNumber,
            seed,
            atlasTextureId,
            gameCatalogs,
            player,
            simulationStepSeconds);
        for (const Actor& actor : level.world.actors())
        {
            npcScripts.forget(actor.id);
        }
        level = std::move(next);
        prepareLevel();
    }

    void Game::prepareLevel()
    {
        const Actor* playerActor = level.world.findActor(level.world.playerId());
        if (playerActor == nullptr)
        {
            throw std::logic_error("The game could not initialise its camera");
        }
        cameraController =
            makeCameraController(level.map, playerActor->body.bounds, gameCatalogs.camera.deadZone);
        queueNavigationFill(level.map, level.world, simulationStepSeconds);
    }

    void Game::update(const InputIntentions& intentions, float deltaTime, FrameProfile* profile)
    {
        Actor* player = level.world.findActor(level.world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The game has no player");
        }

        player->intentions = intentions;
        updateWorldSimulation(level.map, level.world, deltaTime, npcScripts, profile);

        if (level.world.levelComplete())
        {
            changeLevel(LevelChange::NextLevel);
            return;
        }
        if (level.world.playerDefeated())
        {
            currentRunSeed = nextRunSeed(currentRunSeed);
            changeLevel(LevelChange::NewRun);
            return;
        }

        player = level.world.findActor(level.world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The game has no player after lifecycle update");
        }
        followTarget(cameraControllerValue(), level.map, player->body.bounds);
        updateWorldPresentation(level.map, level.world, deltaTime, presentation, cameraShake);
    }

    glm::vec2 Game::playerAimDirection(glm::vec2 screenPosition) const
    {
        const Actor* player = level.world.findActor(level.world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The game has no player");
        }

        return screenToWorld(currentCamera(), screenPosition) - centerOf(player->body.bounds);
    }

    RenderScene Game::buildScene() const
    {
        const Actor* player = level.world.findActor(level.world.playerId());
        if (player == nullptr || !player->sprite.has_value())
        {
            throw std::logic_error("The player is missing its sprite");
        }

        return buildRenderScene(
            level.map, player->sprite.value().textureId, renderCamera(), level.world);
    }

    DebugOverlay Game::debugOverlay(
        float atlasWidth,
        std::optional<glm::vec2> internalCursor,
        std::size_t navigationProfileIndex,
        std::optional<ActorId> lockedMachineActor) const
    {
        NavigationDebugView navigation;
        if (internalCursor.has_value())
        {
            navigation.cursorWorld =
                screenToWorld(currentCamera(), internalCursor.value_or(glm::vec2{0.0F, 0.0F}));
        }
        navigation.profileIndex = navigationProfileIndex;
        for (const auto& [name, definition] : gameCatalogs.actors.definitions)
        {
            const auto* platformer = std::get_if<PlatformerMovementConfig>(&definition.movement);
            if (platformer != nullptr && definition.senses.has_value())
            {
                navigation.namedProfiles.push_back(
                    {name,
                     {.size = definition.bodySize,
                      .movement = *platformer,
                      .stepSeconds = simulationStepSeconds,
                      .climb = definition.surfaceClimb}});
            }
        }
        DebugOverlay overlay = makeDebugOverlay(
            level.world,
            level.map,
            cameraControllerValue(),
            atlasWidth,
            simulationStepSeconds,
            navigation,
            lockedMachineActor);
        for (ActorDebugInfo& actor : overlay.actors)
        {
            const auto definition = level.actorDefinitionNames.find(actor.id.value);
            if (definition != level.actorDefinitionNames.end())
            {
                actor.definitionName = definition->second;
            }
        }
        return overlay;
    }

    std::optional<ActorId> Game::machineActorAt(glm::vec2 internalPosition) const
    {
        const glm::vec2 worldPosition = screenToWorld(currentCamera(), internalPosition);
        for (const Actor& actor : level.world.actors())
        {
            if (actor.machine.has_value() && contains(actor.body.bounds, worldPosition))
            {
                return actor.id;
            }
        }
        return std::nullopt;
    }

    Health Game::playerHealth() const
    {
        const Actor* player = level.world.findActor(level.world.playerId());
        if (player == nullptr || !player->health.has_value())
        {
            throw std::logic_error("The player is missing its health");
        }
        return *player->health;
    }

    Camera Game::currentCamera() const
    {
        return cameraControllerValue().camera;
    }

    const HudIcons& Game::hudIcons() const
    {
        return gameCatalogs.hudIcons;
    }

    const Inventory& Game::playerInventory() const
    {
        const Actor* player = level.world.findActor(level.world.playerId());
        if (player == nullptr || !player->inventory.has_value())
        {
            throw std::logic_error("The player is missing its inventory");
        }
        return *player->inventory;
    }

    const ItemDefinition& Game::itemDefinition(int id) const
    {
        return level.world.itemDefinition(id);
    }

    void Game::useInventoryItem(std::size_t slot)
    {
        WorldRequests requests;
        requests.useItem(level.world.playerId(), slot);
        applyWorldRequests(level.world, requests);
    }

    bool Game::breakTileAt(glm::vec2 internalPosition)
    {
        const glm::vec2 world = screenToWorld(currentCamera(), internalPosition);
        if (world.x < 0.0F || world.y < 0.0F || world.x >= level.map.pixelWidth() ||
            world.y >= level.map.pixelHeight())
        {
            return false;
        }
        return level.map.breakTile(cellAt(level.map.tileSize(), world));
    }

    LevelReload Game::reload(GameContent content)
    {
        validateRoomPieces(content.gameCatalogs, simulationStepSeconds);
        GameLevel fresh = composeLevel(
            content.gameCatalogs.pieces,
            level.number,
            level.seed,
            atlasTextureId,
            content.gameCatalogs,
            composePlayer(content.gameCatalogs, atlasTextureId));
        if (!playerCanReachExit(fresh.map, fresh.world, simulationStepSeconds))
        {
            throw std::invalid_argument(
                std::format(
                    "Level {}: seed {} has no route from the spawn to the exit",
                    level.number,
                    level.seed));
        }
        GameLevel next = level;
        LevelReload result = reloadLevel(
            next, std::move(fresh), matchItemIds(gameCatalogs.items, content.gameCatalogs.items));

        level = std::move(next);
        gameCatalogs = std::move(content.gameCatalogs);
        npcScripts = std::move(content.npcScripts);
        presentation = std::move(content.presentation);
        CameraController& camera = cameraControllerValue();
        camera.deadZoneSize = gameCatalogs.camera.deadZone;
        const Actor* player = level.world.findActor(level.world.playerId());
        if (player != nullptr)
        {
            followTarget(camera, level.map, player->body.bounds);
        }
        queueNavigationFill(level.map, level.world, simulationStepSeconds);
        return result;
    }

    const GameLevel& Game::currentLevel() const
    {
        return level;
    }

    int Game::levelNumber() const
    {
        return level.number;
    }

    std::uint32_t Game::levelSeed() const
    {
        return level.seed;
    }

    std::uint32_t Game::runSeed() const
    {
        return currentRunSeed;
    }

    std::optional<glm::vec2> Game::levelExitScreenPosition() const
    {
        const auto& levelExit = level.world.exit();
        if (!levelExit.has_value())
        {
            return std::nullopt;
        }
        return worldToScreen(renderCamera(), topCenterOf(levelExit.value().bounds));
    }

    namespace
    {
        constexpr float LockedExitHintSeconds = 1.0F;
    }

    std::optional<Sprite> Game::lockedExitHintIcon() const
    {
        const auto& levelExit = level.world.exit();
        if (!levelExit.has_value() || !levelExit->requirement.has_value())
        {
            return std::nullopt;
        }
        const std::optional<float> sinceTouch =
            level.world.secondsSince(levelExit->lastLockedTouchTimeSeconds);
        if (!sinceTouch.has_value() || *sinceTouch > LockedExitHintSeconds)
        {
            return std::nullopt;
        }
        return level.world.itemDefinition(levelExit->requirement->item).icon;
    }

    CameraController& Game::cameraControllerValue()
    {
        if (!cameraController.has_value())
        {
            throw std::logic_error("The game camera is not initialised");
        }
        return *cameraController;
    }

    const CameraController& Game::cameraControllerValue() const
    {
        if (!cameraController.has_value())
        {
            throw std::logic_error("The game camera is not initialised");
        }
        return *cameraController;
    }

    std::vector<LuaScriptDiagnostic> Game::takeScriptDiagnostics()
    {
        std::vector<LuaScriptDiagnostic> diagnostics = npcScripts.takeDiagnostics();
        std::vector<LuaScriptDiagnostic> effects = presentation.takeDiagnostics();
        diagnostics.insert(
            diagnostics.end(),
            std::make_move_iterator(effects.begin()),
            std::make_move_iterator(effects.end()));
        return diagnostics;
    }

    Camera Game::renderCamera() const
    {
        Camera camera = currentCamera();
        const glm::vec2 offset = cameraShake.offset();
        camera.position += glm::vec2{std::round(offset.x), std::round(offset.y)};
        return camera;
    }
}
