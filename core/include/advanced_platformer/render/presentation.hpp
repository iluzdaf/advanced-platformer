#pragma once

#include <vector>

#include "advanced_platformer/render/presentation_scripts.hpp"

namespace advanced_platformer
{
    class CameraShake;
    class PresentationScripts;
    class TileMap;
    class World;

    std::vector<SoundEffect> updateWorldPresentation(
        const TileMap& map,
        World& world,
        float deltaTime,
        PresentationScripts& scripts,
        CameraShake& shake);
}
