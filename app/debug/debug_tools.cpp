#include "debug_tools.hpp"

#include "diagnostics/console_log.hpp"
#include "console_ui.hpp"
#include "diagnostics/debug_overlay.hpp"
#include "debug_overlay_ui.hpp"
#include "debug_tool_visibility.hpp"
#include "frame_profile_ui.hpp"
#include "frame_selection.hpp"
#include "machine_graph_ui.hpp"

#include <optional>

#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/timing/frame_profile.hpp"

namespace advanced_platformer
{
    FramePlotRequest drawDebugTools(
        DebugTools& tools,
        std::optional<ActorId>& machineActor,
        const FrameProfile& profile,
        const DebugOverlay& overlay,
        const std::optional<WindowViewport>& viewport,
        const DebugToolVisibility& visibility,
        const ConsoleLog& console,
        bool paused)
    {
        if (machineActor.has_value() &&
            (!overlay.machine.has_value() || overlay.machine->actor != *machineActor))
        {
            machineActor.reset();
        }
        if (visibility.worldAndCameraOverlay || visibility.actorText ||
            visibility.navigationCacheText)
        {
            drawDebugOverlay(
                overlay,
                viewport,
                visibility.worldAndCameraOverlay,
                visibility.actorText,
                visibility.navigationCacheText,
                visibility.stateMachine);
        }
        if (visibility.stateMachine)
        {
            drawMachineGraph(tools.machineEditors, overlay.machine, machineActor.has_value());
        }
        if (visibility.console)
        {
            drawConsole(tools.consoleView, console);
        }
        recordFrameForPlot(tools.frameHistory, tools.frameSelection, profile, paused);
        return drawFrameProfile(
            tools.frameHistory,
            tools.frameSelection,
            tools.frameAxes,
            visibility.frameProfileDetails);
    }
}
