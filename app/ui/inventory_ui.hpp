#pragma once

#include <cstddef>
#include <optional>

namespace advanced_platformer
{
    class Game;
    struct SpriteRegion;
    struct Texture;
    struct WindowViewport;

    bool drawInventoryButton(
        const Texture& atlas,
        const SpriteRegion& bagIcon,
        const WindowViewport& viewport);

    std::optional<std::size_t> drawInventory(
        const Game& game,
        const Texture& atlas,
        const WindowViewport& viewport);
}
