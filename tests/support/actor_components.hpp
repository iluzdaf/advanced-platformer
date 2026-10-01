#pragma once

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/inventory/inventory.hpp"
#include "advanced_platformer/movement/flying_movement.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/render/animation.hpp"
#include "advanced_platformer/render/sprite.hpp"
#include "advanced_platformer/world/world.hpp"

namespace tests
{
    // Reads an actor back out of a World, or one of its components out of an actor or a World.
    // Each fails the test when the actor or component is missing, rather than letting it
    // dereference nothing.

    inline advanced_platformer::Actor& actor(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        advanced_platformer::Actor* result = world.findActor(id);
        REQUIRE(result != nullptr);
        return *result;
    }

    // The actor the world treats as its player.
    inline advanced_platformer::Actor& player(advanced_platformer::World& world)
    {
        return actor(world, world.playerId());
    }

    inline advanced_platformer::Health& health(advanced_platformer::Actor& actor)
    {
        std::optional<advanced_platformer::Health>& component = actor.health;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no health");
        }
        return *component;
    }

    inline advanced_platformer::Health& health(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        return health(actor(world, id));
    }

    inline advanced_platformer::Inventory& inventory(advanced_platformer::Actor& actor)
    {
        std::optional<advanced_platformer::Inventory>& component = actor.inventory;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no inventory");
        }
        return *component;
    }

    inline advanced_platformer::Inventory& inventory(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        return inventory(actor(world, id));
    }

    inline advanced_platformer::Sprite& sprite(advanced_platformer::Actor& actor)
    {
        std::optional<advanced_platformer::Sprite>& component = actor.sprite;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no sprite");
        }
        return *component;
    }

    inline advanced_platformer::Sprite& sprite(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        return sprite(actor(world, id));
    }

    inline advanced_platformer::Animator& animator(advanced_platformer::Actor& actor)
    {
        std::optional<advanced_platformer::Animator>& component = actor.animator;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no animator");
        }
        return *component;
    }

    inline advanced_platformer::Animator& animator(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        return animator(actor(world, id));
    }

    inline advanced_platformer::NpcBrain& brain(advanced_platformer::Actor& actor)
    {
        std::optional<advanced_platformer::NpcBrain>& component = actor.brain;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no NPC brain");
        }
        return *component;
    }

    inline advanced_platformer::NpcBrain& brain(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        return brain(actor(world, id));
    }

    inline advanced_platformer::NpcPerception& perception(advanced_platformer::Actor& actor)
    {
        std::optional<advanced_platformer::NpcPerception>& component = actor.perception;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no NPC perception");
        }
        return *component;
    }

    inline advanced_platformer::NpcPerception& perception(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        return perception(actor(world, id));
    }

    inline advanced_platformer::NpcMachine& machine(advanced_platformer::Actor& actor)
    {
        std::optional<advanced_platformer::NpcMachine>& component = actor.machine;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no machine");
        }
        return *component;
    }

    inline advanced_platformer::NpcMachine& machine(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        return machine(actor(world, id));
    }

    inline advanced_platformer::BiteAttack& bite(advanced_platformer::Actor& actor)
    {
        std::optional<advanced_platformer::BiteAttack>& component = actor.bite;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no bite");
        }
        return *component;
    }

    inline advanced_platformer::BiteAttack& bite(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        return bite(actor(world, id));
    }

    inline advanced_platformer::RangedWeapon& rangedWeapon(advanced_platformer::Actor& actor)
    {
        std::optional<advanced_platformer::RangedWeapon>& component = actor.rangedWeapon;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no ranged weapon");
        }
        return *component;
    }

    inline advanced_platformer::RangedWeapon& rangedWeapon(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        return rangedWeapon(actor(world, id));
    }

    inline advanced_platformer::ContactDamage& contactDamage(advanced_platformer::Actor& actor)
    {
        std::optional<advanced_platformer::ContactDamage>& component = actor.contactDamage;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no contact damage");
        }
        return *component;
    }

    inline advanced_platformer::ContactDamage& contactDamage(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        return contactDamage(actor(world, id));
    }

    inline advanced_platformer::SurfaceClimb& surfaceClimb(advanced_platformer::Actor& actor)
    {
        std::optional<advanced_platformer::SurfaceClimb>& component = actor.surfaceClimb;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no surface climb");
        }
        return *component;
    }

    inline advanced_platformer::SurfaceClimb& surfaceClimb(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        return surfaceClimb(actor(world, id));
    }

    inline advanced_platformer::PathFollower& pathFollower(advanced_platformer::Actor& actor)
    {
        std::optional<advanced_platformer::PathFollower>& component = actor.pathFollower;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no path follower");
        }
        return *component;
    }

    inline advanced_platformer::PathFollower& pathFollower(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        return pathFollower(actor(world, id));
    }

    inline advanced_platformer::Patrol& patrol(advanced_platformer::Actor& actor)
    {
        std::optional<advanced_platformer::Patrol>& component = actor.patrol;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no patrol");
        }
        return *component;
    }

    inline advanced_platformer::Patrol& patrol(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        return patrol(actor(world, id));
    }

    inline advanced_platformer::FlyingMovement& flyingMovement(advanced_platformer::Actor& actor)
    {
        std::optional<advanced_platformer::FlyingMovement>& component = actor.flyingMovement;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no flying movement");
        }
        return *component;
    }

    inline advanced_platformer::FlyingMovement& flyingMovement(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        return flyingMovement(actor(world, id));
    }

    inline advanced_platformer::PlatformerMovement& platformerMovement(
        advanced_platformer::Actor& actor)
    {
        std::optional<advanced_platformer::PlatformerMovement>& component =
            actor.platformerMovement;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no platformer movement");
        }
        return *component;
    }

    inline advanced_platformer::PlatformerMovement& platformerMovement(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        return platformerMovement(actor(world, id));
    }

    inline advanced_platformer::NpcSenses& senses(advanced_platformer::Actor& actor)
    {
        std::optional<advanced_platformer::NpcSenses>& component = actor.senses;
        if (!component.has_value())
        {
            throw std::logic_error("The test actor has no senses");
        }
        return *component;
    }
}
