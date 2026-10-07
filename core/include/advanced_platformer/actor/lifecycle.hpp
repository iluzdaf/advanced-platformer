#pragma once

namespace advanced_platformer
{
    class World;
    class WorldRequests;

    void updateLifeState(
        World& world,
        WorldRequests& requests,
        float deltaTime,
        float deathDuration = 0.4F);
}
