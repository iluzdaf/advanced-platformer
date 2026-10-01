#pragma once

#include <optional>

#include "graphics/display_viewport.hpp"

namespace advanced_platformer
{
    struct DebugOverlay;

    void drawDebugOverlay(
        const DebugOverlay& scene,
        const std::optional<WindowViewport>& viewport,
        bool showWorldAndCamera,
        bool showActorText,
        bool showNavigationCacheText,
        bool showStateMachine);
}
