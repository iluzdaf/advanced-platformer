#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/inventory/item.hpp"
#include "advanced_platformer/level/placements.hpp"

namespace advanced_platformer
{
    struct ItemCatalog
    {
        std::map<std::string, ItemDefinition> definitions;
    };

    void validateItemCatalog(const ItemCatalog& catalog);
    ItemCatalog parseItemCatalog(std::string_view text, std::string_view sourceName);
    ItemCatalog loadItemCatalog(const std::filesystem::path& path);
    void validateItemAtlasRegions(
        const ItemCatalog& catalog,
        glm::ivec2 atlasSize,
        std::string_view sourceName);
    const ItemDefinition& itemDefinition(const ItemCatalog& catalog, const std::string& name);
    ItemStack composeItemStack(const ItemCatalog& catalog, const NamedItemStack& stack);
    std::vector<ItemDefinition> composeItems(const ItemCatalog& catalog, int textureId);
}
