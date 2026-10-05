#pragma once

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <type_traits>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_attacks.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/inventory/inventory.hpp"
#include "advanced_platformer/movement/flying_movement.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/pounce.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/render/animation.hpp"
#include "advanced_platformer/render/sprite.hpp"
#include "advanced_platformer/world/world.hpp"

namespace tests
{
    inline advanced_platformer::Actor& actor(
        advanced_platformer::World& world,
        advanced_platformer::ActorId id)
    {
        advanced_platformer::Actor* result = world.findActor(id);
        REQUIRE(result != nullptr);
        return *result;
    }

    inline advanced_platformer::Actor& player(advanced_platformer::World& world)
    {
        return actor(world, world.playerId());
    }

    template <class T> std::optional<T>& componentSlot(advanced_platformer::Actor& actor);

    template <>
    inline std::optional<advanced_platformer::Health>& componentSlot<advanced_platformer::Health>(
        advanced_platformer::Actor& actor)
    {
        return actor.health;
    }

    template <>
    inline std::optional<advanced_platformer::Inventory>& componentSlot<
        advanced_platformer::Inventory>(advanced_platformer::Actor& actor)
    {
        return actor.inventory;
    }

    template <>
    inline std::optional<advanced_platformer::Sprite>& componentSlot<advanced_platformer::Sprite>(
        advanced_platformer::Actor& actor)
    {
        return actor.sprite;
    }

    template <>
    inline std::optional<advanced_platformer::Animator>& componentSlot<
        advanced_platformer::Animator>(advanced_platformer::Actor& actor)
    {
        return actor.animator;
    }

    template <>
    inline std::optional<advanced_platformer::NpcBrain>& componentSlot<
        advanced_platformer::NpcBrain>(advanced_platformer::Actor& actor)
    {
        return actor.brain;
    }

    template <>
    inline std::optional<advanced_platformer::NpcPerception>& componentSlot<
        advanced_platformer::NpcPerception>(advanced_platformer::Actor& actor)
    {
        return actor.perception;
    }

    template <>
    inline std::optional<advanced_platformer::NpcMachine>& componentSlot<
        advanced_platformer::NpcMachine>(advanced_platformer::Actor& actor)
    {
        return actor.machine;
    }

    template <>
    inline std::optional<advanced_platformer::SurfaceClimb>& componentSlot<
        advanced_platformer::SurfaceClimb>(advanced_platformer::Actor& actor)
    {
        return actor.surfaceClimb;
    }

    template <>
    inline std::optional<advanced_platformer::PathFollower>& componentSlot<
        advanced_platformer::PathFollower>(advanced_platformer::Actor& actor)
    {
        return actor.pathFollower;
    }

    template <>
    inline std::optional<advanced_platformer::Patrol>& componentSlot<advanced_platformer::Patrol>(
        advanced_platformer::Actor& actor)
    {
        return actor.patrol;
    }

    template <>
    inline std::optional<advanced_platformer::FlyingMovement>& componentSlot<
        advanced_platformer::FlyingMovement>(advanced_platformer::Actor& actor)
    {
        return actor.flyingMovement;
    }

    template <>
    inline std::optional<advanced_platformer::PlatformerMovement>& componentSlot<
        advanced_platformer::PlatformerMovement>(advanced_platformer::Actor& actor)
    {
        return actor.platformerMovement;
    }

    template <>
    inline std::optional<advanced_platformer::NpcSenses>& componentSlot<
        advanced_platformer::NpcSenses>(advanced_platformer::Actor& actor)
    {
        return actor.senses;
    }

    template <class T>
    constexpr bool IsAttack = std::is_same_v<T, advanced_platformer::BiteAttack> ||
                              std::is_same_v<T, advanced_platformer::RangedWeapon> ||
                              std::is_same_v<T, advanced_platformer::ContactDamage> ||
                              std::is_same_v<T, advanced_platformer::Pounce>;

    template <class T> T& component(advanced_platformer::Actor& actor)
    {
        if constexpr (IsAttack<T>)
        {
            T* attack = advanced_platformer::findAttack<T>(actor);
            if (attack == nullptr)
            {
                throw std::logic_error("The test actor has no attack of that kind");
            }
            return *attack;
        }
        else
        {
            std::optional<T>& slot = componentSlot<T>(actor);
            if (!slot.has_value())
            {
                throw std::logic_error("The test actor has no such component");
            }
            return *slot;
        }
    }

    template <class T>
    T& component(advanced_platformer::World& world, advanced_platformer::ActorId id)
    {
        return component<T>(actor(world, id));
    }
}
