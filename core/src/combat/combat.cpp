#include "advanced_platformer/combat/combat.hpp"

#include <stdexcept>

#include "advanced_platformer/math/validation.hpp"

namespace advanced_platformer
{
    bool areOpponents(Team first, Team second)
    {
        return (first == Team::Player && second == Team::Enemy) ||
               (first == Team::Enemy && second == Team::Player);
    }

    void validateRangedWeapon(const RangedWeapon& weapon)
    {
        requireSeconds(weapon.phaseTimeRemaining, "Ranged weapon phase time remaining");
        if (weapon.damage <= 0 || !isFinitePositive(weapon.projectileSize) ||
            !isFinitePositive(weapon.projectileSpeed) ||
            !isFinitePositive(weapon.projectileLifetime) ||
            !isFinitePositive(weapon.shootDuration) || !isFinitePositive(weapon.recoveryDuration) ||
            !isFinitePositive(weapon.projectileSprite.region.size) ||
            weapon.projectilesPerShot <= 0 || !isFiniteNonNegative(weapon.spreadDegrees) ||
            weapon.spreadDegrees >= 180.0F)
        {
            throw std::invalid_argument("Ranged weapon data is invalid");
        }
    }
}
