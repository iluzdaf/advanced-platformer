#pragma once
#include <optional>

#include "advanced_platformer/inventory/item.hpp"
#include "advanced_platformer/physics/body.hpp"
#include "advanced_platformer/render/sprite.hpp"

namespace advanced_platformer
{
    struct Pickup
    {
        // Falls at the default rates and comes to rest on tiles.
        Body body;
        ItemStack stack;
        // Without an override, rendering uses the item's inventory icon.
        std::optional<Sprite> sprite = std::nullopt;
        // As Actor::screenVisibility.
        std::optional<float> screenVisibility = std::nullopt;
    };

    class TileMap;
    class World;
    class WorldRequests;

    // Checks the pickup itself; World also checks that its item exists.
    void validatePickup(const Pickup& pickup);

    void updatePickupMovement(const TileMap& map, World& world, float deltaTime);

    void updatePickups(const World& world, WorldRequests& requests);
}
