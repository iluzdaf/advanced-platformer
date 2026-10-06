#pragma once

#include <cstddef>

#include "debug/debug_tool_visibility.hpp"
#include "game/level_requests.hpp"
#include "game/play_control.hpp"

namespace advanced_platformer
{
    enum class ApplicationCommand
    {
        Quit,
        ToggleInventory,
        TogglePause,
        StepSimulation,
        RestartGame,
        RestartLevel,
        RerollLevel,
        ToggleDebugOverlay,
        NextDebugBody,
        BreakTile,
        ToggleFrameProfileDetails,
        ToggleWorldAndCameraOverlay,
        ToggleActorText,
        ToggleNavigationCacheText,
        ToggleStateMachine,
        ToggleConsole
    };

    struct ApplicationContext
    {
        PlayControl play;
        LevelRequests levelRequests;
        bool showDebugOverlay = false;
        DebugToolVisibility debugToolVisibility;
        std::size_t debugBodyIndex = 0;
        bool breakTileRequested = false;
        bool quitRequested = false;
    };

    void applyCommand(ApplicationContext& context, ApplicationCommand command);
}
