#pragma once

#include <optional>

#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/inventory/inventory.hpp"
#include "advanced_platformer/movement/flying_movement.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/pounce.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/physics/body.hpp"
#include "advanced_platformer/render/animation.hpp"
#include "advanced_platformer/render/sprite.hpp"

namespace advanced_platformer
{
    struct Health
    {
        int current = 1;
        int maximum = 1;
    };

    enum class LifeState
    {
        Alive,
        Dying
    };

    struct Actor
    {
        ActorId id;
        Body body;
        InputIntentions intentions;
        std::optional<PlatformerMovement> platformerMovement;
        std::optional<FlyingMovement> flyingMovement;
        std::optional<SurfaceClimb> surfaceClimb;
        std::optional<Pounce> pounce;
        Facing facing = Facing::Right;

        LifeState life = LifeState::Alive;
        float deathTimeRemaining = 0.0F;
        std::optional<double> lastDamageTimeSeconds;

        std::optional<Sprite> sprite;
        std::optional<Animator> animator;
        std::optional<float> screenVisibility;
        std::optional<Health> health;
        std::optional<Inventory> inventory;
        Team team = Team::Neutral;
        std::optional<RangedWeapon> rangedWeapon;
        std::optional<BiteAttack> bite;
        std::optional<ContactDamage> contactDamage;
        std::optional<NpcBrain> brain;
        std::optional<NpcPerception> perception;
        std::optional<NpcMachine> machine;
        std::optional<NpcSenses> senses;
        std::optional<Patrol> patrol;
        std::optional<PathFollower> pathFollower;
    };
}
