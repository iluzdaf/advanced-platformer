#include "console_ui.hpp"

#include "console_log.hpp"
#include "debug_ui_layout.hpp"

#include <imgui.h>

namespace advanced_platformer
{
    namespace
    {
        constexpr float WindowHeightFraction = 1.0F / 3.0F;
        constexpr ImVec4 ErrorColour = {1.0F, 0.45F, 0.4F, 1.0F};
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
        ImGui::SetNextWindowBgAlpha(0.75F);
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                           ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                           ImGuiWindowFlags_NoSavedSettings;
        if (!ImGui::Begin("Console", nullptr, Flags))
        {
            ImGui::End();
            return;
        }
        if (log.entries().empty())
        {
            ImGui::TextDisabled("Nothing logged yet.");
        }
        for (const ConsoleEntry& entry : log.entries())
        {
            if (entry.level == ConsoleLevel::Error)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ErrorColour);
            }
            ImGui::TextWrapped("%s", entry.text.c_str());
            if (entry.level == ConsoleLevel::Error)
            {
                ImGui::PopStyleColor();
            }
        }
        const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
        if (log.written() != view.shownWritten && atBottom)
        {
            ImGui::SetScrollHereY(1.0F);
        }
        view.shownWritten = log.written();
        ImGui::End();
    }
}
