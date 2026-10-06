#include "hud_draw.hpp"

#include <imgui.h>

#include "graphics/sprite_renderer.hpp"
#include "advanced_platformer/render/sprite.hpp"

namespace advanced_platformer
{
    namespace
    {
        constexpr ImU32 ShadowColour = IM_COL32(0, 0, 0, 220);
        constexpr float ShadowOffset = 1.0F;
    }

    void drawShadowedText(
        ImDrawList& drawList,
        ImVec2 position,
        ImU32 colour,
        const char* text,
        float wrapWidth)
    {
        ImFont* font = ImGui::GetFont();
        const float size = ImGui::GetFontSize();
        drawList.AddText(
            font,
            size,
            {position.x + ShadowOffset, position.y + ShadowOffset},
            ShadowColour,
            text,
            nullptr,
            wrapWidth);
        drawList.AddText(font, size, position, colour, text, nullptr, wrapWidth);
    }

    void drawAtlasRegion(
        ImDrawList& drawList,
        const Texture& atlas,
        const SpriteRegion& region,
        ImVec2 topLeft,
        ImVec2 bottomRight)
    {
        const float width = static_cast<float>(atlas.width);
        const float height = static_cast<float>(atlas.height);
        drawList.AddImage(
            static_cast<ImTextureID>(atlas.handle),
            topLeft,
            bottomRight,
            {region.position.x / width, region.position.y / height},
            {(region.position.x + region.size.x) / width,
             (region.position.y + region.size.y) / height});
    }
}
