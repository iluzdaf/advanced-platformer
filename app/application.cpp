#include "application.hpp"

#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

#include <glm/vec2.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>

#include "content/asset_watcher.hpp"
#include "content/game_content.hpp"
#include "debug/console_log.hpp"
#include "debug/debug_tools.hpp"
#include "debug/frame_profile_ui.hpp"
#include "game/game.hpp"
#include "game/level_reload.hpp"
#include "graphics/display_viewport.hpp"
#include "graphics/game_window.hpp"
#include "graphics/imgui_session.hpp"
#include "graphics/sprite_renderer.hpp"
#include "ui/interface_ui.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/render/render_scene.hpp"
#include "lua_npc_scripts.hpp"
#include "lua_script_diagnostic.hpp"
#include "advanced_platformer/timing/fixed_step.hpp"
#include "advanced_platformer/timing/frame_profile.hpp"
#include "advanced_platformer/timing/stopwatch.hpp"

namespace advanced_platformer
{
    namespace
    {
#ifdef ADVANCED_PLATFORMER_SOURCE_ASSETS
        constexpr const char* AssetDirectory = ADVANCED_PLATFORMER_SOURCE_ASSETS;
        constexpr bool WatchAssets = true;
#else
        constexpr const char* AssetDirectory = "assets";
        constexpr bool WatchAssets = false;
#endif
        constexpr float AssetPollSeconds = 0.25F;
        constexpr glm::vec2 InitialAimDirection = {1.0F, 0.0F};

        struct ApplicationContext
        {
            InputState input;
            glm::vec2 aimDirection = InitialAimDirection;
            bool showDebugOverlay = false;
            DebugToolVisibility debugToolVisibility;
            std::size_t debugBodyIndex = 0;
            bool breakTileRequested = false;
            bool inventoryOpen = false;
            bool simulationPaused = false;
            bool stepRequested = false;
            bool playInterrupted = false;
            bool restartRequested = false;
            bool restartLevelRequested = false;
            bool rerollLevelRequested = false;
        };

        struct Atlas
        {
            int textureId = 0;
            Texture texture;
        };

        ApplicationContext& contextOf(GLFWwindow* window)
        {
            return *static_cast<ApplicationContext*>(glfwGetWindowUserPointer(window));
        }

        void toggleInventory(ApplicationContext& context)
        {
            context.inventoryOpen = !context.inventoryOpen;
            context.playInterrupted = true;
        }

        void clearAttackButtons(InputState& input)
        {
            input.clearButton(InputButton::PrimaryAttack);
            input.clearButton(InputButton::SecondaryAttack);
        }

        std::filesystem::path atlasPath(const std::filesystem::path& assetDirectory)
        {
            return assetDirectory / "textures" / "sprites.png";
        }
    }

    namespace
    {
        bool DebugToolVisibility::* debugToolForKey(int key)
        {
            switch (key)
            {
            case GLFW_KEY_1:
                return &DebugToolVisibility::frameProfileDetails;
            case GLFW_KEY_2:
                return &DebugToolVisibility::worldAndCameraOverlay;
            case GLFW_KEY_3:
                return &DebugToolVisibility::actorText;
            case GLFW_KEY_4:
                return &DebugToolVisibility::navigationCacheText;
            case GLFW_KEY_5:
                return &DebugToolVisibility::stateMachine;
            case GLFW_KEY_6:
                return &DebugToolVisibility::console;
            default:
                return nullptr;
            }
        }

        void pressCommandKey(GLFWwindow* window, ApplicationContext& context, int key)
        {
            switch (key)
            {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                return;
            case GLFW_KEY_Q:
                toggleInventory(context);
                return;
            case GLFW_KEY_P:
                context.simulationPaused = !context.simulationPaused;
                context.playInterrupted = true;
                return;
            case GLFW_KEY_PERIOD:
                if (context.simulationPaused)
                {
                    context.stepRequested = true;
                }
                return;
            case GLFW_KEY_R:
                context.restartRequested = true;
                return;
            case GLFW_KEY_F5:
                context.restartLevelRequested = true;
                return;
            case GLFW_KEY_F7:
                context.rerollLevelRequested = true;
                return;
            case GLFW_KEY_F1:
                context.showDebugOverlay = !context.showDebugOverlay;
                return;
            case GLFW_KEY_N:
                ++context.debugBodyIndex;
                return;
            case GLFW_KEY_B:
                if (context.showDebugOverlay)
                {
                    context.breakTileRequested = true;
                }
                return;
            default:
                break;
            }

            bool DebugToolVisibility::* tool = debugToolForKey(key);
            if (tool != nullptr && context.showDebugOverlay)
            {
                context.debugToolVisibility.*tool = !(context.debugToolVisibility.*tool);
            }
        }

        std::optional<InputButton> buttonForKey(int key)
        {
            switch (key)
            {
            case GLFW_KEY_A:
            case GLFW_KEY_LEFT:
                return InputButton::Left;
            case GLFW_KEY_D:
            case GLFW_KEY_RIGHT:
                return InputButton::Right;
            case GLFW_KEY_W:
            case GLFW_KEY_UP:
            case GLFW_KEY_SPACE:
                return InputButton::Jump;
            case GLFW_KEY_DOWN:
                return InputButton::Down;
            default:
                return std::nullopt;
            }
        }

        void handleKey(GLFWwindow* window, int key, int, int action, int)
        {
            ApplicationContext& context = contextOf(window);
            if (action == GLFW_PRESS)
            {
                pressCommandKey(window, context, key);
            }
            if (action != GLFW_PRESS && action != GLFW_RELEASE)
            {
                return;
            }

            const std::optional<InputButton> button = buttonForKey(key);
            if (button.has_value())
            {
                context.input.setButton(*button, action == GLFW_PRESS);
            }
        }

        std::optional<InputButton> attackForMouseButton(int button)
        {
            switch (button)
            {
            case GLFW_MOUSE_BUTTON_LEFT:
                return InputButton::PrimaryAttack;
            case GLFW_MOUSE_BUTTON_RIGHT:
                return InputButton::SecondaryAttack;
            default:
                return std::nullopt;
            }
        }

        void handleMouseButton(GLFWwindow* window, int button, int action, int)
        {
            if (action != GLFW_PRESS && action != GLFW_RELEASE)
            {
                return;
            }

            const std::optional<InputButton> attack = attackForMouseButton(button);
            if (attack.has_value())
            {
                contextOf(window).input.setButton(*attack, action == GLFW_PRESS);
            }
        }

        void restartCompletedGame(ApplicationContext& context, Game& game)
        {
            if (!game.complete())
            {
                return;
            }

            game.restart();
            context.inventoryOpen = false;
            context.aimDirection = InitialAimDirection;
            context.playInterrupted = true;
        }

        void restartLevel(ApplicationContext& context, Game& game, ConsoleLog& console)
        {
            try
            {
                game.restartLevel();
                context.playInterrupted = true;
            }
            catch (const std::exception& error)
            {
                console.write(
                    ConsoleLevel::Error,
                    std::format("Could not restart the level: {}", error.what()));
            }
        }

        void rerollLevel(ApplicationContext& context, Game& game, ConsoleLog& console)
        {
            try
            {
                if (!game.rerollLevel())
                {
                    console.write(ConsoleLevel::Info, "A completed game has no level to reroll");
                    return;
                }

                context.playInterrupted = true;
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

        void applyLevelRequests(ApplicationContext& context, Game& game, ConsoleLog& console)
        {
            if (context.restartRequested)
            {
                restartCompletedGame(context, game);
                context.restartRequested = false;
            }
            if (context.restartLevelRequested)
            {
                restartLevel(context, game, console);
                context.restartLevelRequested = false;
            }
            if (context.rerollLevelRequested)
            {
                rerollLevel(context, game, console);
                context.rerollLevelRequested = false;
            }
        }

        bool assetsChanged(std::optional<AssetWatcher>& assetWatcher, Stopwatch& assetPollClock)
        {
            if (!assetWatcher.has_value() || assetPollClock.elapsedSeconds() < AssetPollSeconds)
            {
                return false;
            }

            assetPollClock.lapSeconds();
            return assetWatcher->poll();
        }

        void reloadContent(
            Game& game,
            SpriteRenderer& renderer,
            Atlas& atlas,
            const std::filesystem::path& assetDirectory,
            ConsoleLog& console)
        {
            try
            {
                const Image image = loadImage(atlasPath(assetDirectory).string());
                const LevelReload reload =
                    game.reload(loadGameContent(assetDirectory, {image.width, image.height}));
                renderer.replaceTexture(atlas.textureId, image);
                atlas.texture = renderer.texture(atlas.textureId);
                console.write(ConsoleLevel::Info, describeReload(reload));
            }
            catch (const std::exception& error)
            {
                console.write(
                    ConsoleLevel::Error, std::format("Could not reload content: {}", error.what()));
            }
        }

        void applyInterfaceRequests(
            ApplicationContext& context,
            Game& game,
            const InterfaceRequests& requests)
        {
            if (requests.toggleInventory)
            {
                toggleInventory(context);
            }
            if (requests.useInventorySlot.has_value())
            {
                game.useInventoryItem(*requests.useInventorySlot);
            }
        }

        void breakRequestedTile(
            ApplicationContext& context,
            Game& game,
            const std::optional<glm::vec2>& internalCursor)
        {
            if (!context.breakTileRequested)
            {
                return;
            }

            if (internalCursor.has_value())
            {
                game.breakTileAt(*internalCursor);
            }
            context.breakTileRequested = false;
        }

        void selectClickedMachineActor(
            ApplicationContext& context,
            const Game& game,
            DebugTools& debugTools,
            const std::optional<glm::vec2>& internalCursor)
        {
            if (!context.showDebugOverlay || !context.debugToolVisibility.stateMachine ||
                !internalCursor.has_value() || !ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                ImGui::GetIO().WantCaptureMouse)
            {
                return;
            }

            const std::optional<ActorId> clicked = game.machineActorAt(*internalCursor);
            if (!clicked.has_value())
            {
                return;
            }

            if (debugTools.machineActor == clicked)
            {
                debugTools.machineActor.reset();
            }
            else
            {
                debugTools.machineActor = clicked;
            }
            clearAttackButtons(context.input);
        }

        std::optional<glm::vec2> gameplayCursor(
            const ApplicationContext& context,
            const std::optional<glm::vec2>& internalCursor)
        {
            if (ImGui::GetIO().WantCaptureMouse || context.inventoryOpen)
            {
                return std::nullopt;
            }
            return internalCursor;
        }

        void clearBlockedInput(
            ApplicationContext& context,
            bool paused,
            const std::optional<glm::vec2>& gameCursor)
        {
            if (paused || context.playInterrupted || ImGui::GetIO().WantCaptureKeyboard)
            {
                context.input = {};
            }
            else if (!gameCursor.has_value())
            {
                clearAttackButtons(context.input);
            }
        }

        InputIntentions playerIntentions(
            ApplicationContext& context,
            const Game& game,
            const std::optional<glm::vec2>& gameCursor)
        {
            InputIntentions intentions = context.input.consumeIntentions();
            if (gameCursor.has_value())
            {
                const glm::vec2 aimDirection = game.playerAimDirection(*gameCursor);
                if (aimDirection != glm::vec2{0.0F, 0.0F})
                {
                    context.aimDirection = aimDirection;
                }
            }
            else
            {
                intentions.primaryAttackPressed = false;
                intentions.secondaryAttackPressed = false;
            }
            intentions.aimDirection = context.aimDirection;
            return intentions;
        }

        void advanceSimulation(
            ApplicationContext& context,
            Game& game,
            FixedStep& fixedStep,
            const std::optional<glm::vec2>& gameCursor,
            bool paused,
            FrameProfile& profile)
        {
            const auto step = [&](float deltaTime)
            { game.update(playerIntentions(context, game, gameCursor), deltaTime, &profile); };

            if (!paused && !context.playInterrupted)
            {
                const Stopwatch simulationWatch;
                const FixedStepResult stepped = fixedStep.advance(profile.frameSeconds, step);
                profile.simulationTicks = static_cast<int>(stepped.updates);
                profile.simulationSeconds = simulationWatch.elapsedSeconds();
                return;
            }

            fixedStep.reset();
            context.playInterrupted = false;
            if (context.stepRequested && !context.inventoryOpen && !game.complete())
            {
                const Stopwatch simulationWatch;
                step(static_cast<float>(fixedStep.stepSeconds()));
                profile.simulationTicks = 1;
                profile.simulationSeconds = simulationWatch.elapsedSeconds();
            }
        }

        void writeScriptDiagnostics(Game& game, ConsoleLog& console)
        {
            for (const LuaScriptDiagnostic& diagnostic : game.takeScriptDiagnostics())
            {
                writeScriptDiagnostic(console, diagnostic);
            }
        }

        void renderGame(
            const Game& game,
            SpriteRenderer& renderer,
            glm::ivec2 framebufferSize,
            FrameProfile& profile)
        {
            const Stopwatch sceneWatch;
            const RenderScene scene = game.buildScene();
            profile.sceneSeconds = sceneWatch.elapsedSeconds();

            const Stopwatch renderWatch;
            renderer.render(scene, framebufferSize.x, framebufferSize.y);
            profile.renderSeconds = renderWatch.elapsedSeconds();
        }

        void applyFramePlotRequest(ApplicationContext& context, FramePlotRequest request)
        {
            if (request == FramePlotRequest::None)
            {
                return;
            }

            const bool requestedPause = request == FramePlotRequest::Pause;
            if (context.simulationPaused != requestedPause)
            {
                context.simulationPaused = requestedPause;
                context.playInterrupted = true;
                context.input = {};
            }
        }
    }

    int runApplication()
    {
        ConsoleLog console(std::cerr);
        const GameWindow window(
            "Advanced Platformer",
            {960, 540},
            [&console](std::string message)
            { console.write(ConsoleLevel::Error, std::move(message)); });
        ApplicationContext context;
        glfwSetWindowUserPointer(window.handle(), &context);
        glfwSetKeyCallback(window.handle(), handleKey);
        glfwSetMouseButtonCallback(window.handle(), handleMouseButton);
        const ImGuiSession imgui(window.handle());

        const std::filesystem::path assetDirectory = AssetDirectory;
        SpriteRenderer renderer;
        Atlas atlas;
        atlas.textureId = renderer.loadTexture(loadImage(atlasPath(assetDirectory).string()));
        atlas.texture = renderer.texture(atlas.textureId);
        FixedStep fixedStep;
        GameContent content =
            loadGameContent(assetDirectory, {atlas.texture.width, atlas.texture.height});
        Game game(
            atlas.textureId,
            std::move(content.levelCatalog),
            std::move(content.gameCatalogs),
            std::move(content.npcScripts),
            std::move(content.presentation),
            static_cast<float>(fixedStep.stepSeconds()));
        DebugTools debugTools;
        Stopwatch frameClock;
        std::optional<AssetWatcher> assetWatcher;
        if (WatchAssets)
        {
            assetWatcher.emplace(assetDirectory);
        }
        Stopwatch assetPollClock;

        while (!window.shouldClose())
        {
            glfwPollEvents();
            imgui.beginFrame();

            applyLevelRequests(context, game, console);
            if (assetsChanged(assetWatcher, assetPollClock))
            {
                reloadContent(game, renderer, atlas, assetDirectory, console);
                context.playInterrupted = true;
            }

            const WindowReading reading = window.read();
            const std::optional<WindowViewport> windowViewport =
                makeWindowViewport(reading.size, reading.framebufferSize);

            FrameProfile profile;
            profile.frameSeconds = frameClock.lapSeconds();
            const Stopwatch interfaceWatch;
            const InterfaceRequests interfaceRequests = drawInterface(
                game,
                atlas.texture,
                windowViewport,
                context.inventoryOpen,
                context.simulationPaused);
            profile.interfaceSeconds = interfaceWatch.elapsedSeconds();
            applyInterfaceRequests(context, game, interfaceRequests);

            const std::optional<glm::vec2> internalCursor =
                windowToInternal(reading.cursor, reading.size, reading.framebufferSize);
            breakRequestedTile(context, game, internalCursor);
            selectClickedMachineActor(context, game, debugTools, internalCursor);
            const std::optional<glm::vec2> gameCursor = gameplayCursor(context, internalCursor);

            const bool paused =
                context.inventoryOpen || game.complete() || context.simulationPaused;
            clearBlockedInput(context, paused, gameCursor);
            advanceSimulation(context, game, fixedStep, gameCursor, paused, profile);
            context.stepRequested = false;

            writeScriptDiagnostics(game, console);
            renderGame(game, renderer, reading.framebufferSize, profile);

            if (context.showDebugOverlay)
            {
                const FramePlotRequest plotRequest = drawDebugTools(
                    debugTools,
                    profile,
                    game.debugOverlay(
                        static_cast<float>(atlas.texture.width),
                        internalCursor,
                        context.debugBodyIndex,
                        debugTools.machineActor),
                    windowViewport,
                    context.debugToolVisibility,
                    console,
                    paused);
                applyFramePlotRequest(context, plotRequest);
            }
            imgui.render();
            window.present();
        }

        return EXIT_SUCCESS;
    }
}
