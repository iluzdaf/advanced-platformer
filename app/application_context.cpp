#include "application_context.hpp"

#include "debug/debug_tool_visibility.hpp"
#include "game/level_requests.hpp"

namespace advanced_platformer
{
    namespace
    {
        void toggleDebugTool(ApplicationContext& context, bool DebugToolVisibility::* tool)
        {
            if (context.showDebugOverlay)
            {
                context.debugToolVisibility.*tool = !(context.debugToolVisibility.*tool);
            }
        }
    }

    void applyCommand(ApplicationContext& context, ApplicationCommand command)
    {
        switch (command)
        {
        case ApplicationCommand::Quit:
            context.quitRequested = true;
            return;
        case ApplicationCommand::ToggleInventory:
            context.play.toggleInventory();
            return;
        case ApplicationCommand::TogglePause:
            context.play.togglePause();
            return;
        case ApplicationCommand::StepSimulation:
            context.play.requestStep();
            return;
        case ApplicationCommand::RestartLevel:
            context.levelRequests.restartLevel = true;
            return;
        case ApplicationCommand::RerollLevel:
            context.levelRequests.rerollLevel = true;
            return;
        case ApplicationCommand::ToggleDebugOverlay:
            context.showDebugOverlay = !context.showDebugOverlay;
            return;
        case ApplicationCommand::NextDebugBody:
            ++context.debugBodyIndex;
            return;
        case ApplicationCommand::BreakTile:
            if (context.showDebugOverlay)
            {
                context.debugCursor.breakTileRequested = true;
            }
            return;
        case ApplicationCommand::ToggleFrameProfileDetails:
            toggleDebugTool(context, &DebugToolVisibility::frameProfileDetails);
            return;
        case ApplicationCommand::ToggleWorldAndCameraOverlay:
            toggleDebugTool(context, &DebugToolVisibility::worldAndCameraOverlay);
            return;
        case ApplicationCommand::ToggleActorText:
            toggleDebugTool(context, &DebugToolVisibility::actorText);
            return;
        case ApplicationCommand::ToggleNavigationCacheText:
            toggleDebugTool(context, &DebugToolVisibility::navigationCacheText);
            return;
        case ApplicationCommand::ToggleStateMachine:
            toggleDebugTool(context, &DebugToolVisibility::stateMachine);
            return;
        case ApplicationCommand::ToggleConsole:
            toggleDebugTool(context, &DebugToolVisibility::console);
            return;
        }
    }
}
