#include "advanced_platformer/inventory/item_use.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/inventory/inventory.hpp"
#include "advanced_platformer/inventory/item.hpp"
#include "advanced_platformer/world/world.hpp"

namespace advanced_platformer
{
    bool hasAmmo(const World& world, const Actor& actor)
    {
        if (!actor.equippedWeapon.has_value() || !actor.inventory.has_value())
        {
            return false;
        }
        const ItemDefinition& weapon = world.itemDefinition(actor.equippedWeapon->item);
        return weapon.ammo.has_value() && actor.inventory->count(*weapon.ammo) > 0;
    }

    void equipWeapon(Actor& actor, const ItemDefinition& weapon)
    {
        holsterWeapon(actor);
        actor.equippedWeapon = EquippedWeapon{weapon.id, std::move(actor.primaryAttack)};
        actor.primaryAttack = weapon.weapon;
    }

    void holsterWeapon(Actor& actor)
    {
        if (!actor.equippedWeapon.has_value())
        {
            return;
        }
        actor.primaryAttack = std::move(actor.equippedWeapon->holstered);
        actor.equippedWeapon.reset();
    }

    namespace
    {
        bool toggleWeapon(Actor& actor, const ItemDefinition& definition)
        {
            const bool wasEquipped =
                actor.equippedWeapon.has_value() && actor.equippedWeapon->item == definition.id;
            holsterWeapon(actor);
            if (wasEquipped || !actor.inventory.has_value() || !definition.ammo.has_value() ||
                actor.inventory->count(*definition.ammo) == 0)
            {
                return wasEquipped;
            }
            equipWeapon(actor, definition);
            return true;
        }
    }

    bool useItem(World& world, ActorId id, std::size_t slot)
    {
        Actor* actor = world.findActor(id);
        if (actor == nullptr || actor->life != LifeState::Alive || !actor->inventory.has_value() ||
            slot >= actor->inventory->slots().size())
        {
            return false;
        }
        const auto& selected = actor->inventory->slots()[slot];
        if (!selected.has_value())
        {
            return false;
        }
        const ItemDefinition& definition = world.itemDefinition(selected->item);
        if (definition.weapon.has_value())
        {
            return toggleWeapon(*actor, definition);
        }
        switch (definition.effect)
        {
        case ItemEffect::None:
            return false;
        case ItemEffect::Heal:
            if (!actor->health.has_value() || actor->health->current >= actor->health->maximum)
            {
                return false;
            }
            actor->health->current +=
                std::min(definition.effectAmount, actor->health->maximum - actor->health->current);
            actor->inventory->removeFromSlot(slot, 1);
            return true;
        }
        return false;
    }
}
