#pragma once

#include <array>
#include <variant>

#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/movement/pounce.hpp"

namespace advanced_platformer
{
    using Attack = std::variant<BiteAttack, RangedWeapon, ContactDamage, Pounce>;

    enum class AttackSlot
    {
        Primary,
        Secondary
    };

    inline constexpr std::array<AttackSlot, 2> AttackSlots{
        AttackSlot::Primary,
        AttackSlot::Secondary};
}
