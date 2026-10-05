#include "advanced_platformer/actor/lifecycle.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/world/world.hpp"
#include "advanced_platformer/world/world_requests.hpp"

namespace advanced_platformer
{
    void updateLifeState(
        World& world,
        WorldRequests& requests,
        float deltaTime,
        float deathDuration)
    {
        requireSeconds(deltaTime, "Life states time step");
        if (!isFinitePositive(deathDuration))
        {
            throw std::invalid_argument("Life states require a finite, positive death duration");
        }

        std::vector<ActorId> actorsAlreadyDying;
        for (const Actor& actor : world.actors())
        {
            if (actor.life == LifeState::Dying)
            {
                actorsAlreadyDying.push_back(actor.id);
            }
        }

        for (const auto& request : requests.damageRequests)
        {
            Actor* actor = world.findActor(request.target);
            if (actor == nullptr || actor->life != LifeState::Alive || !actor->health.has_value())
            {
                continue;
            }

            actor->health->current = std::max(0, actor->health->current - request.amount);
            actor->lastDamageTimeSeconds = world.simulationTimeSeconds();
            if (request.knockback.has_value())
            {
                actor->body.velocity = *request.knockback;
                if (actor->platformerMovement.has_value())
                {
                    actor->platformerMovement->grounded = false;
                }
            }
            if (actor->health->current == 0)
            {
                actor->life = LifeState::Dying;
                actor->deathTimeRemaining = deathDuration;
                actor->intentions = {};
            }
        }
        requests.damageRequests.clear();

        for (const ActorId id : actorsAlreadyDying)
        {
            Actor* actor = world.findActor(id);
            if (actor == nullptr)
            {
                continue;
            }

            actor->deathTimeRemaining = std::max(0.0F, actor->deathTimeRemaining - deltaTime);
            if (actor->deathTimeRemaining > 0.0F)
            {
                continue;
            }

            if (id == world.playerId())
            {
                world.respawnPlayer();
            }
            else
            {
                requests.removalRequests.push_back(id);
            }
        }
    }
}
