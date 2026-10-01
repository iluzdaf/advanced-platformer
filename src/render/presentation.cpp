#include "advanced_platformer/render/presentation.hpp"

#include "advanced_platformer/render/animation_system.hpp"
#include "advanced_platformer/render/cover_fade.hpp"

namespace advanced_platformer
{
    void updateWorldPresentation(const TileMap& map, World& world, float deltaTime)
    {
        updateWorldAnimations(world, deltaTime);
        updateCoverFades(map, world, deltaTime);
    }
}
