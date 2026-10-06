#include "level_number_ui.hpp"

#include <format>
#include <string>

#include <imgui.h>

#include "advanced_platformer/math/coordinates.hpp"
#include "graphics/display_viewport.hpp"
#include "ui/hud_draw.hpp"
#include "ui/hud_layout.hpp"

namespace advanced_platformer
{
    void drawLevelNumber(int levelNumber, const WindowViewport& viewport)
    {
        const std::string text = std::format("Level {}", levelNumber);
        const float centerX =
            viewport.topLeft.x + (InternalViewportSize.x * 0.5F * viewport.scale.x);
        const float top = viewport.topLeft.y + (HudMargin * viewport.scale.y);
        drawCenteredText(*ImGui::GetForegroundDrawList(), centerX, top, text.c_str());
    }
}
