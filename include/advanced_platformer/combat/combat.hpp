#pragma once

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/render/sprite.hpp"

namespace advanced_platformer
{
    enum class Team
    {
        Neutral,
        Player,
        Enemy
    };

    bool areOpponents(Team first, Team second);

    enum class RangedPhase
    {
        Ready,
        Shoot,
        Recovery
    };

    struct RangedWeapon
    {
        int damage = 1;
        glm::vec2 projectileSize = {4.0F, 2.0F};
        float projectileSpeed = 180.0F;
        float projectileLifetime = 2.0F;
        float shootDuration = 0.15F;
        float recoveryDuration = 0.20F;
        bool breaksTiles = false;

        RangedPhase phase = RangedPhase::Ready;
        float phaseTimeRemaining = 0.0F;
        std::optional<double> lastFiredTimeSeconds;
        Sprite projectileSprite = {0, {{0.0F, 0.0F}, {4.0F, 2.0F}}};
    };

    enum class BitePhase
    {
        Ready,
        Windup,
        Active,
        Recovery
    };

    struct BiteAttack
    {
        int damage = 1;
        glm::vec2 hitboxSize = {10.0F, 8.0F};
        float reach = 4.0F;
        float windupDuration = 0.12F;
        float activeDuration = 0.08F;
        float recoveryDuration = 0.30F;

        BitePhase phase = BitePhase::Ready;
        float phaseTimeRemaining = 0.0F;
        std::vector<ActorId> actorsHit;
    };

    struct Knockback
    {
        float speed = 150.0F;
        float lift = 120.0F;
    };

    struct ContactDamage
    {
        int damage = 1;
        std::optional<Knockback> knockback;
        bool active = false;
        std::vector<ActorId> actorsHit;
    };

    struct Projectile
    {
        Aabb bounds;
        glm::vec2 velocity = {0.0F, 0.0F};
        int damage = 1;
        float lifetimeRemaining = 1.0F;
        std::optional<ActorId> owner;
        Team team = Team::Neutral;
        Sprite sprite;
        bool breaksTiles = false;
    };

    enum class ProjectileBurstCause
    {
        Impact,
        LifetimeExpired
    };

    struct ProjectileBurst
    {
        ProjectileBurstCause cause = ProjectileBurstCause::Impact;
        glm::vec2 center = {0.0F, 0.0F};
        glm::vec2 direction = {1.0F, 0.0F};
        Sprite sprite;
        float duration = 0.1F;
        float lifetimeRemaining = 0.1F;
    };
}
