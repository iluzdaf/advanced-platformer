#pragma once

#include <optional>
#include <variant>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/combat/attack.hpp"
#include "advanced_platformer/input/input_state.hpp"

namespace advanced_platformer
{
    inline std::optional<Attack>& attackIn(Actor& actor, AttackSlot slot)
    {
        return slot == AttackSlot::Primary ? actor.primaryAttack : actor.secondaryAttack;
    }

    inline const std::optional<Attack>& attackIn(const Actor& actor, AttackSlot slot)
    {
        return slot == AttackSlot::Primary ? actor.primaryAttack : actor.secondaryAttack;
    }

    inline bool attackPressed(const InputIntentions& intentions, AttackSlot slot)
    {
        return slot == AttackSlot::Primary ? intentions.primaryAttackPressed
                                           : intentions.secondaryAttackPressed;
    }

    template <class T> std::optional<AttackSlot> slotHolding(const Actor& actor)
    {
        for (const AttackSlot slot : AttackSlots)
        {
            const std::optional<Attack>& attack = attackIn(actor, slot);
            if (attack.has_value() && std::holds_alternative<T>(*attack))
            {
                return slot;
            }
        }
        return std::nullopt;
    }

    template <class T> T* findAttack(Actor& actor)
    {
        for (const AttackSlot slot : AttackSlots)
        {
            std::optional<Attack>& attack = attackIn(actor, slot);
            if (attack.has_value())
            {
                if (T* found = std::get_if<T>(&*attack))
                {
                    return found;
                }
            }
        }
        return nullptr;
    }

    template <class T> const T* findAttack(const Actor& actor)
    {
        for (const AttackSlot slot : AttackSlots)
        {
            const std::optional<Attack>& attack = attackIn(actor, slot);
            if (attack.has_value())
            {
                if (const T* found = std::get_if<T>(&*attack))
                {
                    return found;
                }
            }
        }
        return nullptr;
    }
}
