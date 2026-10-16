#include "advanced_platformer/inventory/item.hpp"

#include <stdexcept>

#include "advanced_platformer/combat/combat.hpp"

namespace advanced_platformer
{
    void validateItemDefinition(const ItemDefinition& definition)
    {
        if (definition.id <= 0 || definition.name.empty() || definition.maximumStack <= 0 ||
            (definition.effect == ItemEffect::Heal && definition.effectAmount <= 0) ||
            (definition.effect == ItemEffect::None && definition.effectAmount != 0))
        {
            throw std::invalid_argument(
                "Item definitions require an ID, name and valid stack/effect");
        }
        if (definition.weapon.has_value() != definition.ammo.has_value() ||
            (definition.weapon.has_value() &&
             (definition.effect != ItemEffect::None || *definition.ammo <= 0 ||
              *definition.ammo == definition.id)))
        {
            throw std::invalid_argument(
                "A weapon item needs another item as its ammo and no other effect");
        }
        if (definition.weapon.has_value())
        {
            validateRangedWeapon(*definition.weapon);
        }
    }
}
