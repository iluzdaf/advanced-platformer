#pragma once

#include <optional>
#include <string>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/movement/flying_movement.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/render/sprite.hpp"

#include "animation_catalog.hpp"
#include "machine_catalog.hpp"

namespace advanced_platformer
{
    // Initial component settings; each composition creates fresh runtime state.
    struct ActorDefinition
    {
        // Content declares it; composition rejects a size left at zero.
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
        // Presence creates the brain, perception, and path follower with these senses, and
        // requires a machine.
        std::optional<NpcSenses> senses;
        // The machine in the machine catalog that decides what the NPC does. Every NPC has
        // one, and only an NPC: it requires senses.
        std::string machine;
        // Reuse the engine's attack settings. Composition resets their phase/timer state;
        // JSON exposes only configuration fields, never those runtime fields.
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
