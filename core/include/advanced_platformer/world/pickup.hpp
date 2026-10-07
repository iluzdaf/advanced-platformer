#pragma once
#include <cstddef>
#include <optional>

#include "advanced_platformer/inventory/item.hpp"
#include "advanced_platformer/physics/body.hpp"
#include "advanced_platformer/render/sprite.hpp"

namespace advanced_platformer
{
    struct Pickup
    {
        Body body;
        ItemStack stack;
        std::optional<Sprite> sprite = std::nullopt;
        std::optional<float> screenVisibility = std::nullopt;
        std::optional<std::size_t> placement = std::nullopt;
    };

    class TileMap;
    class World;
    class WorldRequests;

    void validatePickup(const Pickup& pickup);

    void updatePickupMovement(const TileMap& map, World& world, float deltaTime);

    void updatePickups(const World& world, WorldRequests& requests);
}
