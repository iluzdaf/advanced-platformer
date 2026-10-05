#pragma once

#include <utility>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/combat/attack.hpp"
#include "advanced_platformer/inventory/inventory.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/flying_movement.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/navigation/platformer_cells.hpp"
#include "advanced_platformer/navigation/route.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/render/animation.hpp"
#include "advanced_platformer/render/sprite.hpp"
#include "support/tile_size.hpp"

namespace tests
{
    class ActorBuilder
    {
    public:
        class Sized;
        class Placed;
        class Thinking;

        static Sized sized(glm::vec2 size);

        ActorBuilder onTeam(advanced_platformer::Team team) &&
        {
            built.team = team;
            return std::move(*this);
        }

        Thinking thinking(advanced_platformer::NpcSenses senses) &&;

        ActorBuilder patrolling(glm::vec2 firstFeet, glm::vec2 secondFeet) &&
        {
            built.patrol = advanced_platformer::Patrol{firstFeet, secondFeet, true};
            return std::move(*this);
        }

        ActorBuilder withHealth(int current, int maximum) &&
        {
            built.health = advanced_platformer::Health{current, maximum};
            return std::move(*this);
        }

        ActorBuilder withInventory(advanced_platformer::Inventory inventory) &&
        {
            built.inventory = std::move(inventory);
            return std::move(*this);
        }

        ActorBuilder withSprite(advanced_platformer::Sprite sprite) &&
        {
            built.sprite = sprite;
            return std::move(*this);
        }

        ActorBuilder withAnimator(advanced_platformer::Animator animator) &&
        {
            built.animator = std::move(animator);
            return std::move(*this);
        }

        ActorBuilder climbing(advanced_platformer::SurfaceClimbConfig config = {}) &&
        {
            built.surfaceClimb = advanced_platformer::SurfaceClimb{config};
            return std::move(*this);
        }

        ActorBuilder withPrimary(advanced_platformer::Attack attack) &&
        {
            built.primaryAttack = std::move(attack);
            return std::move(*this);
        }

        ActorBuilder withSecondary(advanced_platformer::Attack attack) &&
        {
            built.secondaryAttack = std::move(attack);
            return std::move(*this);
        }

        operator advanced_platformer::Actor() &&
        {
            return std::move(built);
        }

    protected:
        explicit ActorBuilder(advanced_platformer::Actor actor)
            : built(std::move(actor))
        {
        }

        advanced_platformer::Actor built;
    };

    class ActorBuilder::Thinking : public ActorBuilder
    {
    public:
        Thinking running(advanced_platformer::NpcStateMachine machine) &&
        {
            built.machine = advanced_platformer::startNpcMachine(std::move(machine));
            return std::move(*this);
        }

    private:
        friend class ActorBuilder;

        explicit Thinking(advanced_platformer::Actor actor)
            : ActorBuilder(std::move(actor))
        {
        }
    };

    inline ActorBuilder::Thinking ActorBuilder::thinking(advanced_platformer::NpcSenses senses) &&
    {
        built.brain = advanced_platformer::NpcBrain{};
        built.perception = advanced_platformer::NpcPerception{};
        built.senses = senses;
        built.pathFollower = advanced_platformer::PathFollower{};
        built.machine =
            advanced_platformer::startNpcMachine({"idle", {{"idle", {"test", "idle"}}}, {}});
        return Thinking(std::move(built));
    }

    class ActorBuilder::Placed
    {
    public:
        ActorBuilder platforming(advanced_platformer::PlatformerMovementConfig config = {}) &&
        {
            built.platformerMovement = advanced_platformer::PlatformerMovement{config};
            return ActorBuilder(std::move(built));
        }

        ActorBuilder flying(float speed) &&
        {
            built.flyingMovement = advanced_platformer::FlyingMovement{speed};
            return ActorBuilder(std::move(built));
        }

    private:
        friend class ActorBuilder::Sized;

        explicit Placed(const advanced_platformer::Aabb& bounds)
        {
            built.body.bounds = bounds;
        }

        advanced_platformer::Actor built;
    };

    class ActorBuilder::Sized
    {
    public:
        Placed at(glm::vec2 topLeft) &&
        {
            return Placed({topLeft, size});
        }

        Placed atFeet(glm::vec2 feet) &&
        {
            return Placed(advanced_platformer::boxStandingOn(feet, size));
        }

        Placed inCell(advanced_platformer::Cell cell) &&
        {
            return Placed(advanced_platformer::boxInCell(TileSize, cell, size));
        }

        Placed restingAt(advanced_platformer::RouteLocation location) &&
        {
            return Placed(advanced_platformer::boundsAtSurface(TileSize, location, size));
        }

    private:
        friend class ActorBuilder;

        explicit Sized(glm::vec2 bodySize)
            : size(bodySize)
        {
        }

        glm::vec2 size;
    };

    inline ActorBuilder::Sized ActorBuilder::sized(glm::vec2 size)
    {
        return Sized(size);
    }
}
