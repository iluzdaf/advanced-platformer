#pragma once

#include <optional>

#include "debug/console_ui.hpp"
#include "debug/frame_axes.hpp"
#include "debug/frame_profile_ui.hpp"
#include "debug/frame_selection.hpp"
#include "debug/machine_graph_ui.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/timing/frame_profile.hpp"

namespace advanced_platformer
{
    class ConsoleLog;
    struct DebugOverlay;
    struct WindowViewport;

    struct DebugToolVisibility
    {
        bool frameProfileDetails = false;
        bool worldAndCameraOverlay = false;
        bool actorText = false;
        bool navigationCacheText = false;
        bool stateMachine = false;
        bool console = false;
    };

    struct DebugTools
    {
        FrameHistory frameHistory;
        FrameSelection frameSelection;
        FrameAxes frameAxes;
        MachineGraphEditors machineEditors;
        std::optional<ActorId> machineActor;
        ConsoleView consoleView;
    };

    FramePlotRequest drawDebugTools(
        DebugTools& tools,
        const FrameProfile& profile,
        const DebugOverlay& overlay,
        const std::optional<WindowViewport>& viewport,
        const DebugToolVisibility& visibility,
        const ConsoleLog& console,
        bool paused);
}
