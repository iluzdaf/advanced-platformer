#pragma once

#include <utility>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/world/world.hpp"

namespace tests
{
    // Adds the actor as the world's player, respawning where it stands.
    inline advanced_platformer::ActorId addPlayer(
        advanced_platformer::World& world,
        advanced_platformer::Actor actor)
    {
        const glm::vec2 feet = advanced_platformer::feetOf(actor.body.bounds);
        const advanced_platformer::ActorId id = world.addActor(std::move(actor));
        world.setPlayer(id, feet);
        return id;
    }
}
