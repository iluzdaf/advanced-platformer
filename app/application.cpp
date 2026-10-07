#include "application.hpp"
#include "application_context.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <utility>

#include <glm/vec2.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>

#include "content/content_session.hpp"
#include "content/game_content.hpp"
#include "debug/console_log.hpp"
#include "debug/debug_cursor.hpp"
#include "debug/debug_tools.hpp"
#include "debug/frame_profile_ui.hpp"
#include "game/game.hpp"
#include "game/level_requests.hpp"
#include "game/play_control.hpp"
#include "graphics/display_viewport.hpp"
#include "graphics/game_window.hpp"
#include "graphics/imgui_session.hpp"
#include "graphics/sprite_renderer.hpp"
#include "ui/interface_ui.hpp"
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

        ApplicationContext& contextOf(GLFWwindow* window)
        {
            return *static_cast<ApplicationContext*>(glfwGetWindowUserPointer(window));
        }

        std::optional<ApplicationCommand> commandForKey(int key)
        {
            switch (key)
            {
            case GLFW_KEY_ESCAPE:
                return ApplicationCommand::Quit;
            case GLFW_KEY_Q:
                return ApplicationCommand::ToggleInventory;
            case GLFW_KEY_P:
                return ApplicationCommand::TogglePause;
            case GLFW_KEY_PERIOD:
                return ApplicationCommand::StepSimulation;
            case GLFW_KEY_F5:
                return ApplicationCommand::RestartLevel;
            case GLFW_KEY_F6:
                return ApplicationCommand::RerollLevel;
            case GLFW_KEY_F1:
                return ApplicationCommand::ToggleDebugOverlay;
            case GLFW_KEY_N:
                return ApplicationCommand::NextDebugBody;
            case GLFW_KEY_B:
                return ApplicationCommand::BreakTile;
            case GLFW_KEY_1:
                return ApplicationCommand::ToggleFrameProfileDetails;
            case GLFW_KEY_2:
                return ApplicationCommand::ToggleWorldAndCameraOverlay;
            case GLFW_KEY_3:
                return ApplicationCommand::ToggleActorText;
            case GLFW_KEY_4:
                return ApplicationCommand::ToggleNavigationCacheText;
            case GLFW_KEY_5:
                return ApplicationCommand::ToggleStateMachine;
            case GLFW_KEY_6:
                return ApplicationCommand::ToggleConsole;
            default:
                return std::nullopt;
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
                const std::optional<ApplicationCommand> command = commandForKey(key);
                if (command.has_value())
                {
                    applyCommand(context, *command);
                }
            }
            if (action != GLFW_PRESS && action != GLFW_RELEASE)
            {
                return;
            }

            const std::optional<InputButton> button = buttonForKey(key);
            if (button.has_value())
            {
                context.play.setButton(*button, action == GLFW_PRESS);
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
                contextOf(window).play.setButton(*attack, action == GLFW_PRESS);
            }
        }

        void applyInterfaceRequests(
            ApplicationContext& context,
            Game& game,
            const InterfaceRequests& requests)
        {
            if (requests.toggleInventory)
            {
                context.play.toggleInventory();
            }
            if (requests.useInventorySlot.has_value())
            {
                game.useInventoryItem(*requests.useInventorySlot);
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

        void applyFramePlotRequest(PlayControl& play, FramePlotRequest request)
        {
            if (request != FramePlotRequest::None)
            {
                play.setPaused(request == FramePlotRequest::Pause);
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

        SpriteRenderer renderer;
        ContentSession content(AssetDirectory, renderer, WatchAssets);
        FixedStep fixedStep;
        GameContent loaded = content.load();
        Game game(
            content.atlasTextureId(),
            std::move(loaded.gameCatalogs),
            std::move(loaded.npcScripts),
            std::move(loaded.presentation),
            static_cast<float>(fixedStep.stepSeconds()),
            std::random_device{}());
        DebugTools debugTools;
        Stopwatch frameClock;

        while (!window.shouldClose() && !context.quitRequested)
        {
            glfwPollEvents();
            imgui.beginFrame();

            applyLevelRequests(context.levelRequests, game, context.play, console);
            if (content.assetsChanged())
            {
                content.reload(game, console);
                context.play.interrupt();
            }

            const WindowReading reading = window.read();
            const std::optional<WindowViewport> windowViewport =
                makeWindowViewport(reading.size, reading.framebufferSize);

            FrameProfile profile;
            profile.frameSeconds = frameClock.lapSeconds();
            const Stopwatch interfaceWatch;
            const Texture atlas = content.atlas();
            const InterfaceRequests interfaceRequests = drawInterface(
                game,
                atlas,
                windowViewport,
                context.play.inventoryOpen(),
                context.play.simulationPaused());
            profile.interfaceSeconds = interfaceWatch.elapsedSeconds();
            applyInterfaceRequests(context, game, interfaceRequests);

            const std::optional<glm::vec2> internalCursor =
                windowToInternal(reading.cursor, reading.size, reading.framebufferSize);
            breakRequestedTile(context.debugCursor, game, internalCursor);
            if (context.showDebugOverlay && context.debugToolVisibility.stateMachine)
            {
                selectClickedMachineActor(
                    context.debugCursor,
                    game,
                    context.play,
                    {internalCursor,
                     ImGui::IsMouseClicked(ImGuiMouseButton_Left),
                     ImGui::GetIO().WantCaptureMouse});
            }
            const bool paused = context.play.paused();
            context.play.advance(
                game,
                fixedStep,
                {internalCursor,
                 ImGui::GetIO().WantCaptureMouse,
                 ImGui::GetIO().WantCaptureKeyboard},
                profile);

            writeScriptDiagnostics(game, console);
            renderGame(game, renderer, reading.framebufferSize, profile);

            if (context.showDebugOverlay)
            {
                const FramePlotRequest plotRequest = drawDebugTools(
                    debugTools,
                    context.debugCursor.machineActor,
                    profile,
                    game.debugOverlay(
                        static_cast<float>(atlas.width),
                        internalCursor,
                        context.debugBodyIndex,
                        context.debugCursor.machineActor),
                    windowViewport,
                    context.debugToolVisibility,
                    console,
                    paused);
                applyFramePlotRequest(context.play, plotRequest);
            }
            imgui.render();
            window.present();
        }

        return EXIT_SUCCESS;
    }
}
