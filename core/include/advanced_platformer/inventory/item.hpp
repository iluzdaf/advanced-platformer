#pragma once

#include <string>

#include "advanced_platformer/render/sprite.hpp"

namespace advanced_platformer
{
    using ItemId = int;

    enum class ItemEffect
    {
        None,
        Heal
    };

    struct ItemDefinition
    {
        ItemId id = 0;
        std::string name;
        Sprite icon;
        int maximumStack = 1;
        ItemEffect effect = ItemEffect::None;
        int effectAmount = 0;
    };

    struct ItemStack
    {
        ItemId item = 0;
        int quantity = 1;
    };

    void validateItemDefinition(const ItemDefinition& definition);
}
