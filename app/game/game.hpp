#pragma once

#include <optional>
#include <vector>
#include <cstddef>

#include <glm/vec2.hpp>

#include "debug/debug_overlay.hpp"
#include "game/level_composition.hpp"
#include "content/level_catalog.hpp"
#include "content/game_catalogs.hpp"
#include "content/hud_catalog.hpp"
#include "game/level_reload.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/render/camera.hpp"
#include "advanced_platformer/render/sprite.hpp"
#include "lua_npc_scripts.hpp"
#include "lua_script_diagnostic.hpp"
#include "lua_presentation_script.hpp"

namespace advanced_platformer
{
    constexpr float LockedExitHintSeconds = 1.0F;

    struct FrameProfile;
    struct GameContent;
    struct Health;
    struct InputIntentions;
    struct RenderScene;
    class Inventory;
    struct ItemDefinition;

    class Game
    {
    public:
        Game(
            int textureId,
            LevelCatalog levelCatalog,
            GameCatalogs gameCatalogs,
            LuaNpcScripts npcScripts,
            LuaPresentationScript presentation,
            float simulationStepSeconds);

        void update(
            const InputIntentions& intentions,
            float deltaTime,
            FrameProfile* profile = nullptr);
        glm::vec2 playerAimDirection(glm::vec2 screenPosition) const;
        RenderScene buildScene() const;
        DebugOverlay debugOverlay(
            float atlasWidth,
            std::optional<glm::vec2> internalCursor,
            std::size_t navigationProfileIndex,
            std::optional<ActorId> lockedMachineActor = std::nullopt) const;
        std::optional<ActorId> machineActorAt(glm::vec2 internalPosition) const;
        Health playerHealth() const;
        const Inventory& playerInventory() const;
        const ItemDefinition& itemDefinition(int id) const;
        void useInventoryItem(std::size_t slot);
        bool breakTileAt(glm::vec2 internalPosition);
        void restart();
        void restartLevel();
        LevelReload reload(GameContent content);
        int levelNumber() const;
        bool complete() const;
        std::optional<glm::vec2> levelExitScreenPosition() const;
        bool exitReady() const;
        std::optional<Sprite> lockedExitHintIcon() const;
        const HudIcons& hudIcons() const;
        std::vector<LuaScriptDiagnostic> takeScriptDiagnostics();
        Camera currentCamera() const;
        Camera renderCamera() const;

    private:
        void loadLevel(int levelNumber);
        void replaceLevel(int levelNumber, Actor player);
        void startCamera();
        CameraController& cameraControllerValue();
        const CameraController& cameraControllerValue() const;

        LevelCatalog levelCatalog;
        GameCatalogs gameCatalogs;
        LuaNpcScripts npcScripts;
        LuaPresentationScript presentation;
        GameLevel level;
        std::optional<CameraController> cameraController;
        CameraShake cameraShake;
        int atlasTextureId = 0;
        float simulationStepSeconds = 0.0F;
        bool gameComplete = false;
    };
}
