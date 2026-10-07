#pragma once

#include <cstddef>
#include <optional>

namespace advanced_platformer
{
    class Game;
    struct Texture;
    struct WindowViewport;

    struct InterfaceRequests
    {
        bool toggleInventory = false;
        std::optional<std::size_t> useInventorySlot;
    };

    InterfaceRequests drawInterface(
        const Game& game,
        const Texture& atlas,
        const std::optional<WindowViewport>& viewport,
        bool inventoryOpen,
        bool simulationPaused);
}
