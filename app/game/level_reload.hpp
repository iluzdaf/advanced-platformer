#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "level_composition.hpp"
#include "advanced_platformer/inventory/item.hpp"

namespace advanced_platformer
{
    struct ItemCatalog;
    class World;

    struct LevelReload
    {
        std::size_t kept = 0;
        std::vector<std::string> spawned;
        std::vector<std::string> removed;
    };

    std::map<ItemId, ItemId> matchItemIds(const ItemCatalog& before, const ItemCatalog& after);

    LevelReload reloadLevel(
        GameLevel& live,
        GameLevel fresh,
        const std::map<ItemId, ItemId>& itemIds);

    std::string describeReload(const LevelReload& reload);
}
