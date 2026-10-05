#pragma once

#include <optional>
#include <string>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/movement/flying_movement.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/pounce.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/render/sprite.hpp"

#include "animation_catalog.hpp"
#include "machine_catalog.hpp"

namespace advanced_platformer
{
    struct ActorDefinition
    {
        glm::vec2 bodySize = {0.0F, 0.0F};
        Team team = Team::Neutral;
        Facing facing = Facing::Right;
        SpriteAnchor spriteAnchor = SpriteAnchor::BodyFeet;
        std::string animations;
        std::optional<int> health;
        std::optional<int> inventorySlots;
        std::optional<PlatformerMovementConfig> platformer;
        std::optional<FlyingMovement> flying;
        std::optional<SurfaceClimbConfig> surfaceClimb;
        std::optional<PounceConfig> pounce;
        std::optional<NpcSenses> senses;
        std::string machine;
        std::optional<BiteAttack> bite;
        std::optional<ContactDamage> contactDamage;
        std::optional<RangedWeapon> ranged;
    };

    Actor composeActor(
        const ActorDefinition& definition,
        const AnimationCatalog& animations,
        int textureId,
        glm::vec2 spawnFeet = {},
        std::optional<Patrol> patrol = std::nullopt,
        const MachineCatalog& machines = {});
    void validateActorDefinition(
        const ActorDefinition& definition,
        const AnimationCatalog& animations,
        const MachineCatalog& machines = {});
}
