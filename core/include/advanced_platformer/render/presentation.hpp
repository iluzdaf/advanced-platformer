#pragma once

namespace advanced_platformer
{
    class CameraShake;
    class PresentationScripts;
    class TileMap;
    class World;

    void updateWorldPresentation(
        const TileMap& map,
        World& world,
        float deltaTime,
        PresentationScripts& scripts,
        CameraShake& shake);
}
