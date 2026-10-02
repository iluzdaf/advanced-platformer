#include "console_ui.hpp"

#include "console_log.hpp"
#include "debug_draw.hpp"
#include "debug_ui_layout.hpp"
#include "ui/hud_draw.hpp"

#include <imgui.h>

namespace advanced_platformer
{
    namespace
    {
        constexpr float WindowHeightFraction = 1.0F / 3.0F;
        constexpr ImU32 ConsoleErrorColour = IM_COL32(255, 112, 96, 255);
    }

    void drawConsole(ConsoleView& view, const ConsoleLog& log)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float textPanelWidth =
            DebugTextContentWidth + 2.0F * ImGui::GetStyle().WindowPadding.x;
        const float height = (viewport->WorkSize.y - 2.0F * DebugPanelGap) * WindowHeightFraction;
        const ImVec2 topLeft = {
            viewport->WorkPos.x + FrameProfilePanelWidth + DebugPanelGap,
            viewport->WorkPos.y + viewport->WorkSize.y - DebugPanelGap - height};
        const ImVec2 size = {
            viewport->WorkSize.x - FrameProfilePanelWidth - textPanelWidth - 2.0F * DebugPanelGap,
            height};
        if (size.x <= 0.0F || size.y <= 0.0F)
        {
            return;
        }
        ImGui::SetNextWindowPos(topLeft, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration |
                                           ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoSavedSettings |
                                           ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar;
        if (!ImGui::Begin("Console##overlay", nullptr, Flags))
        {
            ImGui::End();
            return;
        }
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float wrapWidth = ImGui::GetContentRegionAvail().x;
        const ImVec2 start = ImGui::GetCursorScreenPos();
        ImVec2 position = start;
        if (log.entries().empty())
        {
            drawTextLine(*drawList, position, "Nothing logged yet.", TextDetailColour);
        }
        for (const ConsoleEntry& entry : log.entries())
        {
            const ImU32 colour =
                entry.level == ConsoleLevel::Error ? ConsoleErrorColour : TextDetailColour;
            drawShadowedText(*drawList, position, colour, entry.text.c_str(), wrapWidth);
            position.y += ImGui::CalcTextSize(entry.text.c_str(), nullptr, false, wrapWidth).y;
        }
        ImGui::Dummy({wrapWidth, position.y - start.y});
        const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
        if (log.written() != view.shownWritten && atBottom)
        {
            ImGui::SetScrollHereY(1.0F);
        }
        view.shownWritten = log.written();
        ImGui::End();
    }
}
