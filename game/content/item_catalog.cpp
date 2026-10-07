#include "item_catalog.hpp"

#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "content_validation.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glaze/glaze.hpp>

#include "advanced_platformer/inventory/item.hpp"

template <> struct glz::meta<advanced_platformer::ItemEffect>
{
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr std::array keys{"none", "heal"};
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr std::array value{
        advanced_platformer::ItemEffect::None,
        advanced_platformer::ItemEffect::Heal};
};

namespace advanced_platformer
{
    struct ItemJson
    {
        std::string name;
        SpriteJson icon;
        int maximumStack = 1;
        std::optional<ItemEffect> effect;
        std::optional<int> effectAmount;
    };

    struct ItemsJson
    {
        std::map<std::string, ItemJson> items;
    };

    void validateItemCatalog(const ItemCatalog& catalog)
    {
        std::set<ItemId> ids;
        for (const auto& entry : catalog.definitions)
        {
            try
            {
                if (entry.first.empty())
                {
                    throw std::invalid_argument("item name cannot be empty");
                }
                validateItemDefinition(entry.second);
                validateContentSprite(entry.second.icon);
                if (!ids.insert(entry.second.id).second)
                {
                    throw std::invalid_argument("repeated item ID");
                }
            }
            catch (const std::invalid_argument& error)
            {
                failJson({}, fieldPath("items", entry.first), error.what());
            }
        }
    }

    ItemCatalog parseItemCatalog(std::string_view text, std::string_view sourceName)
    {
        const auto file = readContent<ItemsJson>(text, sourceName);
        if (file.items.size() > static_cast<std::size_t>(std::numeric_limits<ItemId>::max()))
        {
            failJson(sourceName, "items", "too many item definitions");
        }
        ItemCatalog catalog;
        for (const auto& [key, json] : file.items)
        {
            ItemDefinition item;
            item.id = static_cast<ItemId>(catalog.definitions.size() + 1);
            item.name = json.name;
            item.icon = spriteFrom(json.icon);
            item.maximumStack = json.maximumStack;
            item.effect = json.effect.value_or(ItemEffect::None);
            item.effectAmount = json.effectAmount.value_or(0);
            catalog.definitions.emplace(key, item);
        }
        validateInFile(sourceName, [&] { validateItemCatalog(catalog); });
        return catalog;
    }

    ItemCatalog loadItemCatalog(const std::filesystem::path& path)
    {
        return parseItemCatalog(loadContentText(path), path.string());
    }

    void validateItemAtlasRegions(
        const ItemCatalog& catalog,
        glm::ivec2 atlasSize,
        std::string_view sourceName)
    {
        for (const auto& [name, definition] : catalog.definitions)
        {
            requireInAtlas(
                definition.icon.region,
                atlasSize,
                sourceName,
                fieldPath(fieldPath("items", name), "icon"));
        }
    }

    const ItemDefinition& itemDefinition(const ItemCatalog& catalog, const std::string& name)
    {
        const auto found = catalog.definitions.find(name);
        if (found == catalog.definitions.end())
        {
            throw std::invalid_argument(std::format("unknown item '{}'", name));
        }
        return found->second;
    }

    ItemStack composeItemStack(const ItemCatalog& catalog, const NamedItemStack& stack)
    {
        if (stack.quantity <= 0)
        {
            throw std::invalid_argument("item quantity must be positive");
        }
        return {itemDefinition(catalog, stack.item).id, stack.quantity};
    }

    std::vector<ItemDefinition> composeItems(const ItemCatalog& catalog, int textureId)
    {
        validateItemCatalog(catalog);
        std::vector<ItemDefinition> result;
        for (const auto& entry : catalog.definitions)
        {
            auto item = entry.second;
            item.icon.textureId = textureId;
            result.push_back(item);
        }
        return result;
    }
}
