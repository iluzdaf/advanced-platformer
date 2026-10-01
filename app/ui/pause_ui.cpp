#include "pause_ui.hpp"

#include <imgui.h>

#include "graphics/display_viewport.hpp"
#include "ui/hud_draw.hpp"
#include "ui/hud_layout.hpp"

namespace advanced_platformer
{
    void drawPauseNotice(const WindowViewport& viewport)
    {
        const ImVec2 topLeft = {
            viewport.topLeft.x + HudMargin * viewport.scale.x,
            viewport.topLeft.y + HudMargin * viewport.scale.y};
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        drawShadowedText(*drawList, topLeft, HudTextColour, "Paused");
    }
}
