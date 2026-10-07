#pragma once

namespace advanced_platformer
{
    class NpcActivityScripts;
    class TileMap;
    class World;
    class WorldRequests;
    struct FrameProfile;

    struct NpcUpdate
    {
        const TileMap& map;
        World& world;
        WorldRequests& requests;
        float deltaTime;
        NpcActivityScripts& scripts;
        FrameProfile* profile;
    };
}
