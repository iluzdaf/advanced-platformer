#pragma once

#include <optional>

#include "advanced_platformer/inventory/item.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/render/sprite.hpp"

namespace advanced_platformer
{
    constexpr float ExitOpenSeconds = 1.0F;

    struct LevelExit
    {
        Aabb bounds;
        std::optional<ItemStack> requirement;
        bool consumeItem = false;
        std::optional<Sprite> sprite;
        std::optional<double> lastLockedTouchTimeSeconds;
        std::optional<double> openedTimeSeconds;
    };

    class World;
    struct Actor;

    void validateLevelExit(const LevelExit& exit);

    bool exitUnlocked(const LevelExit& exit, const Actor& actor);
    bool exitOpening(const World& world);
    void updateLevelExit(World& world);
}
