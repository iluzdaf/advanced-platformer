#include "pickup_catalog.hpp"

#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "content_validation.hpp"
#include "item_catalog.hpp"

#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <glaze/glaze.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/world/pickup.hpp"

namespace advanced_platformer
{
    // pickups.json as written: its member names are the file's keys. Glaze reflects only
    // types with linkage, so these cannot go in an anonymous namespace.
    struct PickupJson
    {
        std::string item;
        int quantity = 0;
        glm::vec2 bodySize{};
        std::optional<SpriteJson> sprite;
    };

    struct PickupsJson
    {
        std::map<std::string, PickupJson> pickups;
    };

    void validatePickupDefinition(const PickupDefinition& definition, const ItemCatalog& items)
    {
        Pickup pickup;
        pickup.body.bounds.size = definition.bodySize;
        pickup.stack = composeItemStack(items, definition.stack);
        pickup.sprite = definition.sprite;
        validatePickup(pickup);
        if (definition.sprite)
        {
            validateContentSprite(*definition.sprite);
        }
    }

    void validatePickupCatalog(const PickupCatalog& catalog, const ItemCatalog& items)
    {
        for (const auto& entry : catalog)
        {
            try
            {
                if (entry.first.empty())
                {
                    throw std::invalid_argument("pickup name cannot be empty");
                }
                validatePickupDefinition(entry.second, items);
            }
            catch (const std::invalid_argument& error)
            {
                failJson({}, fieldPath("pickups", entry.first), error.what());
            }
        }
    }

    PickupCatalog parsePickupCatalog(
        std::string_view text,
        std::string_view sourceName,
        const ItemCatalog& items)
    {
        const auto file = readContent<PickupsJson>(text, sourceName);
        PickupCatalog catalog;
        for (const auto& [name, json] : file.pickups)
        {
            PickupDefinition definition;
            definition.stack = {json.item, json.quantity};
            definition.bodySize = json.bodySize;
            if (json.sprite.has_value())
            {
                definition.sprite = spriteFrom(*json.sprite);
            }
            catalog.emplace(name, definition);
        }
        validateInFile(sourceName, [&] { validatePickupCatalog(catalog, items); });
        return catalog;
    }

    PickupCatalog loadPickupCatalog(const std::filesystem::path& path, const ItemCatalog& items)
    {
        return parsePickupCatalog(loadContentText(path), path.string(), items);
    }

    void validatePickupAtlasRegions(
        const PickupCatalog& catalog,
        glm::ivec2 atlasSize,
        std::string_view sourceName)
    {
        for (const auto& [name, definition] : catalog)
        {
            if (definition.sprite.has_value())
            {
                requireInAtlas(
                    definition.sprite->region,
                    atlasSize,
                    sourceName,
                    fieldPath(fieldPath("pickups", name), "sprite"));
            }
        }
    }

    const PickupDefinition& pickupDefinition(const PickupCatalog& catalog, const std::string& name)
    {
        const auto found = catalog.find(name);
        if (found == catalog.end())
        {
            throw std::invalid_argument(std::format("unknown pickup definition '{}'", name));
        }
        return found->second;
    }

    Pickup composePickup(
        const PickupDefinition& definition,
        const ItemCatalog& items,
        int textureId,
        glm::vec2 spawnFeet)
    {
        validatePickupDefinition(definition, items);
        Pickup result;
        result.body.bounds = boxStandingOn(spawnFeet, definition.bodySize);
        result.stack = composeItemStack(items, definition.stack);
        result.sprite = definition.sprite;
        if (result.sprite)
        {
            result.sprite->textureId = textureId;
        }
        validatePickup(result);
        return result;
    }
}
