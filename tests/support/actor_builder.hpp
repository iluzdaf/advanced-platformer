#pragma once

#include <utility>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/combat/combat.hpp"
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
    // Builds an actor for a test, from the geometry and components the test chooses
    // rather than from shipped content:
    //
    //   ActorBuilder::sized({12.0F, 12.0F})
    //       .inCell({3, 1})
    //       .platforming()
    //       .climbing({60.0F})
    //       .thinking({64.0F, 1.0F})
    //       .running(machine)
    //
    // The build rules, which the chain enforces at compile time:
    //
    // 1. sized() comes first, and offers only a placement: at(), atFeet(), inCell() or
    //    restingAt().
    // 2. A placement offers only a movement: platforming() or flying(). Every actor gets
    //    exactly one, which World requires.
    // 3. After the movement, the optional components may come in any order.
    // 4. thinking() makes the actor an NPC, adding its brain, perception, senses and path
    //    follower together. Only then can running() give it a state machine, which is
    //    started and validated as it is added.
    //
    // The chain converts to an Actor wherever one is expected. It checks nothing beyond
    // its order: World validates the result when the actor is added, so climbing() on a
    // flyer builds, and World rejects it.
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

        // Only a platforming actor can climb. World rejects a climbing flyer.
        ActorBuilder climbing(advanced_platformer::SurfaceClimbConfig config = {}) &&
        {
            built.surfaceClimb = advanced_platformer::SurfaceClimb{config};
            return std::move(*this);
        }

        ActorBuilder biting(advanced_platformer::BiteAttack bite = {}) &&
        {
            built.bite = std::move(bite);
            return std::move(*this);
        }

        ActorBuilder withContactDamage(advanced_platformer::ContactDamage contact = {}) &&
        {
            built.contactDamage = std::move(contact);
            return std::move(*this);
        }

        ActorBuilder shooting(advanced_platformer::RangedWeapon weapon = {}) &&
        {
            built.rangedWeapon = weapon;
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

    // An NPC: everything an ActorBuilder offers, and running().
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
        // Every NPC runs a machine; one that is not given one stands idle.
        built.machine =
            advanced_platformer::startNpcMachine({"idle", {{"idle", {"test", "idle"}}}, {}});
        return Thinking(std::move(built));
    }

    // A sized, placed body waiting for its one movement component.
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

    // A body size waiting for its placement.
    class ActorBuilder::Sized
    {
    public:
        // By its top-left corner.
        Placed at(glm::vec2 topLeft) &&
        {
            return Placed({topLeft, size});
        }

        // By the middle of its bottom edge, where the game places actors.
        Placed atFeet(glm::vec2 feet) &&
        {
            return Placed(advanced_platformer::boxStandingOn(feet, size));
        }

        // Standing in the cell, its feet on the middle of the cell's bottom edge. Every test
        // map has tests::TileSize tiles, so the cell is unambiguous.
        Placed inCell(advanced_platformer::Cell cell) &&
        {
            return Placed(advanced_platformer::boxInCell(TileSize, cell, size));
        }

        // Resting at the location: standing on the cell's floor, flush against its wall, or
        // hanging from its ceiling, where a search would start from it.
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
