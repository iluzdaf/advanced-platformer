#pragma once

#include <imgui.h>

namespace advanced_platformer
{
    struct SpriteRegion;
    struct Texture;

    constexpr ImU32 HudTextColour = IM_COL32(255, 255, 255, 255);

    void drawShadowedText(
        ImDrawList& drawList,
        ImVec2 position,
        ImU32 colour,
        const char* text,
        float wrapWidth = 0.0F);

    void drawAtlasRegion(
        ImDrawList& drawList,
        const Texture& atlas,
        const SpriteRegion& region,
        ImVec2 topLeft,
        ImVec2 bottomRight);
}
