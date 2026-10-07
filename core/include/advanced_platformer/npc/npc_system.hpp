#pragma once

namespace advanced_platformer
{
    class NpcActivityScripts;
    struct FrameProfile;
    class TileMap;
    class World;
    class WorldRequests;

    void updateNpcBehaviour(
        const TileMap& map,
        World& world,
        WorldRequests& requests,
        float deltaTime,
        NpcActivityScripts& scripts,
        FrameProfile* profile = nullptr);
}
