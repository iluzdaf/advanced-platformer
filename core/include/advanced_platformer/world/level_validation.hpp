#pragma once

#include <glm/vec2.hpp>

namespace advanced_platformer
{
    struct Actor;
    struct Pickup;
    class TileMap;
    class World;

    void validateActorPlacement(const TileMap& map, const Actor& actor);
    void validatePickupPlacement(const TileMap& map, const Pickup& pickup);
    void validateLevelPlacements(const TileMap& map, const World& world, int level);

    bool actorCanReach(
        const TileMap& map,
        const Actor& actor,
        glm::vec2 goalFeet,
        float stepSeconds);
    bool playerCanReachExit(const TileMap& map, const World& world, float stepSeconds);
}
