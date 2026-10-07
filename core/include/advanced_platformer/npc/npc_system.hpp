#pragma once

namespace advanced_platformer
{
    class NpcActivityScripts;
    struct FrameProfile;
    class TileMap;
    class World;

    // Chooses each NPC's state and runs its activity through the scripts, searching the
    // world's navigation for paths as needed. Optional profiling records search work
    // directly.
    void updateNpcBehaviour(
        const TileMap& map,
        World& world,
        float deltaTime,
        NpcActivityScripts& scripts,
        FrameProfile* profile = nullptr);
}
