#include "level_data.hpp"

#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "content_validation.hpp"
#include "item_catalog.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glaze/glaze.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/math/coordinates.hpp"

// The type of an object legend entry, defined with the file's other shapes below.
namespace advanced_platformer
{
    enum class LevelObjectType;
}

// A map cell is written as [column, row], with exactly two whole numbers.
template <> struct glz::from<glz::JSON, advanced_platformer::Cell>
{
    template <auto Options>
    static void op(advanced_platformer::Cell& value, auto&& context, auto&& it, auto&& end)
    {
        const auto start = it;
        std::vector<int> numbers;
        parse<JSON>::op<Options>(numbers, context, it, end);
        if (bool(context.error))
        {
            return;
        }
        if (numbers.size() != 2)
        {
            it = start;
            context.error = error_code::syntax_error;
            context.custom_error_message = "expected two whole numbers, [column, row]";
            return;
        }
        value = {numbers[0], numbers[1]};
    }
};

template <>
struct glz::from<glz::JSON, advanced_platformer::LevelObjectType>
    : advanced_platformer::NamedEnumReader<advanced_platformer::LevelObjectType>
{
};

namespace advanced_platformer
{
    enum class LevelObjectType
    {
        Player,
        Actor,
        Pickup,
        Exit
    };

    template <> struct ContentNames<LevelObjectType>
    {
        static constexpr std::array Names{
            std::pair{std::string_view{"player"}, LevelObjectType::Player},
            std::pair{std::string_view{"actor"}, LevelObjectType::Actor},
            std::pair{std::string_view{"pickup"}, LevelObjectType::Pickup},
            std::pair{std::string_view{"exit"}, LevelObjectType::Exit}};
    };

    // A level file as written: its member names are the file's keys. Glaze reflects only types
    // with linkage, so these cannot go in an anonymous namespace. A placement gives its position
    // as a cell or as feet; the reader checks it gives exactly one.
    struct PatrolJson
    {
        std::optional<Cell> firstCell;
        std::optional<glm::vec2> firstFeet;
        std::optional<Cell> secondCell;
        std::optional<glm::vec2> secondFeet;
    };

    struct RequirementJson
    {
        std::string item;
        int quantity = 0;
    };

    struct ActorPlacementJson
    {
        std::string definition;
        std::optional<Cell> spawnCell;
        std::optional<glm::vec2> spawnFeet;
        std::optional<PatrolJson> patrol;
    };

    // A pickup names a definition, or gives its item, quantity and body size inline.
    struct PickupPlacementJson
    {
        std::optional<std::string> definition;
        std::optional<std::string> item;
        std::optional<int> quantity;
        std::optional<glm::vec2> bodySize;
        std::optional<Cell> spawnCell;
        std::optional<glm::vec2> spawnFeet;
    };

    struct ExitPlacementJson
    {
        std::string definition;
        std::optional<Cell> spawnCell;
        std::optional<glm::vec2> spawnFeet;
        std::optional<RequirementJson> requirement;
        std::optional<bool> consumeItem;
        std::optional<int> nextLevel;
    };

    // An object legend entry: a placement without a position, which comes from each map cell
    // marked with its symbol. The reader checks a type uses only its own fields.
    struct ObjectTemplateJson
    {
        LevelObjectType type = LevelObjectType::Player;
        std::optional<std::string> definition;
        std::optional<PatrolJson> patrol;
        std::optional<std::string> item;
        std::optional<int> quantity;
        std::optional<glm::vec2> bodySize;
        std::optional<RequirementJson> requirement;
        std::optional<bool> consumeItem;
        std::optional<int> nextLevel;
    };

    struct LevelJson
    {
        std::map<std::string, std::string> tileLegend;
        std::optional<std::map<std::string, ObjectTemplateJson>> objectLegend;
        std::vector<std::string> map;
        std::optional<Cell> playerSpawnCell;
        std::optional<glm::vec2> playerSpawnFeet;
        std::optional<std::vector<ActorPlacementJson>> actors;
        std::optional<std::vector<PickupPlacementJson>> pickups;
        std::optional<ExitPlacementJson> exit;
    };

    namespace
    {
        LevelPosition positionFrom(
            const std::optional<Cell>& cell,
            const std::optional<glm::vec2>& feet,
            std::string_view cellKey,
            std::string_view feetKey,
            std::string_view sourceName,
            std::string_view path)
        {
            if (cell.has_value() == feet.has_value())
            {
                failJson(
                    sourceName,
                    path,
                    std::format("supply exactly one of '{}' or '{}'", cellKey, feetKey));
            }
            if (cell.has_value())
            {
                return *cell;
            }
            return *feet;
        }

        std::string nameFrom(
            const std::string& name,
            std::string_view description,
            std::string_view sourceName,
            std::string_view path)
        {
            if (name.empty())
            {
                failJson(sourceName, path, std::format("{} cannot be empty", description));
            }
            return name;
        }

        ActorPlacement actorFrom(
            const ActorPlacementJson& json,
            std::string_view sourceName,
            const std::string& path)
        {
            ActorPlacement result;
            result.definitionName = nameFrom(
                json.definition,
                "actor definition name",
                sourceName,
                fieldPath(path, "definition"));
            result.spawn = positionFrom(
                json.spawnCell, json.spawnFeet, "spawnCell", "spawnFeet", sourceName, path);
            if (json.patrol.has_value())
            {
                const PatrolJson& patrol = *json.patrol;
                const std::string patrolPath = fieldPath(path, "patrol");
                result.patrol = PatrolPlacement{
                    positionFrom(
                        patrol.firstCell,
                        patrol.firstFeet,
                        "firstCell",
                        "firstFeet",
                        sourceName,
                        patrolPath),
                    positionFrom(
                        patrol.secondCell,
                        patrol.secondFeet,
                        "secondCell",
                        "secondFeet",
                        sourceName,
                        patrolPath)};
            }
            return result;
        }

        PickupPlacement pickupFrom(
            const PickupPlacementJson& json,
            std::string_view sourceName,
            const std::string& path)
        {
            PickupPlacement result;
            result.spawn = positionFrom(
                json.spawnCell, json.spawnFeet, "spawnCell", "spawnFeet", sourceName, path);
            if (json.definition.has_value())
            {
                if (json.item.has_value() || json.quantity.has_value() || json.bodySize.has_value())
                {
                    failJson(
                        sourceName,
                        path,
                        "use either a pickup definition or an inline item, quantity and bodySize");
                }
                result.definitionName = nameFrom(
                    *json.definition,
                    "pickup definition name",
                    sourceName,
                    fieldPath(path, "definition"));
                return result;
            }
            if (!json.item.has_value())
            {
                failJson(sourceName, path, "missing 'item'");
            }
            if (!json.quantity.has_value())
            {
                failJson(sourceName, path, "missing 'quantity'");
            }
            if (!json.bodySize.has_value())
            {
                failJson(sourceName, path, "missing 'bodySize'");
            }
            result.stack = {
                nameFrom(*json.item, "item name", sourceName, fieldPath(path, "item")),
                *json.quantity};
            result.bodySize = *json.bodySize;
            validatePickupSettings(result, path, sourceName);
            return result;
        }

        ExitPlacement exitFrom(
            const ExitPlacementJson& json,
            std::string_view sourceName,
            const std::string& path)
        {
            ExitPlacement result;
            result.definitionName = json.definition;
            result.spawn = positionFrom(
                json.spawnCell, json.spawnFeet, "spawnCell", "spawnFeet", sourceName, path);
            if (json.requirement.has_value())
            {
                const std::string requirementPath = fieldPath(path, "requirement");
                result.requirement = NamedItemStack{
                    nameFrom(
                        json.requirement->item,
                        "item name",
                        sourceName,
                        fieldPath(requirementPath, "item")),
                    json.requirement->quantity};
            }
            result.consumeItem = json.consumeItem.value_or(false);
            result.nextLevel = json.nextLevel;
            validateExitSettings(result, path, sourceName);
            return result;
        }

        // Rejects any field this template's type does not take.
        void checkTemplateFields(
            const ObjectTemplateJson& json,
            std::string_view sourceName,
            const std::string& path)
        {
            const std::array<std::pair<std::string_view, bool>, 8> given{{
                {"definition", json.definition.has_value()},
                {"patrol", json.patrol.has_value()},
                {"item", json.item.has_value()},
                {"quantity", json.quantity.has_value()},
                {"bodySize", json.bodySize.has_value()},
                {"requirement", json.requirement.has_value()},
                {"consumeItem", json.consumeItem.has_value()},
                {"nextLevel", json.nextLevel.has_value()},
            }};
            std::vector<std::string_view> allowed;
            switch (json.type)
            {
            case LevelObjectType::Player:
                break;
            case LevelObjectType::Actor:
                allowed = {"definition", "patrol"};
                break;
            case LevelObjectType::Pickup:
                allowed = {"definition", "item", "quantity", "bodySize"};
                break;
            case LevelObjectType::Exit:
                allowed = {"definition", "requirement", "consumeItem", "nextLevel"};
                break;
            }
            for (const auto& [key, isGiven] : given)
            {
                if (isGiven && !std::ranges::contains(allowed, key))
                {
                    failJson(sourceName, path, std::format("unknown field '{}'", key));
                }
            }
        }

        std::string requiredDefinition(
            const ObjectTemplateJson& json,
            std::string_view sourceName,
            const std::string& path)
        {
            if (!json.definition.has_value())
            {
                failJson(sourceName, path, "missing 'definition'");
            }
            return *json.definition;
        }

        ActorPlacementJson actorAt(
            const ObjectTemplateJson& json,
            Cell cell,
            std::string_view sourceName,
            const std::string& path)
        {
            return {requiredDefinition(json, sourceName, path), cell, std::nullopt, json.patrol};
        }

        PickupPlacementJson pickupAt(const ObjectTemplateJson& json, Cell cell)
        {
            return {json.definition, json.item, json.quantity, json.bodySize, cell, std::nullopt};
        }

        ExitPlacementJson exitAt(
            const ObjectTemplateJson& json,
            Cell cell,
            std::string_view sourceName,
            const std::string& path)
        {
            return {
                requiredDefinition(json, sourceName, path),
                cell,
                std::nullopt,
                json.requirement,
                json.consumeItem,
                json.nextLevel};
        }

        // Checks every template before any map symbol uses it, and keeps the names they
        // reference, including unused ones, so composition can check them against the
        // catalogs with their legend paths.
        void checkLegendTemplates(
            const std::map<std::string, ObjectTemplateJson>& legend,
            std::string_view sourceName,
            LevelData& result)
        {
            const Cell anywhere{0, 0};
            for (const auto& [symbol, json] : legend)
            {
                const std::string path = fieldPath("objectLegend", symbol);
                checkTemplateFields(json, sourceName, path);
                switch (json.type)
                {
                case LevelObjectType::Player:
                    break;
                case LevelObjectType::Actor: {
                    const ActorPlacement placement =
                        actorFrom(actorAt(json, anywhere, sourceName, path), sourceName, path);
                    result.actorReferences.emplace(
                        fieldPath(path, "definition"), placement.definitionName);
                    break;
                }
                case LevelObjectType::Pickup: {
                    const PickupPlacement placement =
                        pickupFrom(pickupAt(json, anywhere), sourceName, path);
                    if (placement.definitionName.empty())
                    {
                        result.itemReferences.emplace(
                            fieldPath(path, "item"), placement.stack.item);
                    }
                    else
                    {
                        result.pickupReferences.emplace(
                            fieldPath(path, "definition"), placement.definitionName);
                    }
                    break;
                }
                case LevelObjectType::Exit: {
                    const ExitPlacement placement =
                        exitFrom(exitAt(json, anywhere, sourceName, path), sourceName, path);
                    result.exitReferences.emplace(
                        fieldPath(path, "definition"), placement.definitionName);
                    if (placement.requirement.has_value())
                    {
                        result.itemReferences.emplace(
                            fieldPath(path, "requirement.item"), placement.requirement->item);
                    }
                    break;
                }
                }
            }
        }

        std::vector<PlacementOrigin> playerOrigins(
            const std::optional<Cell>& cell,
            const std::optional<glm::vec2>& feet)
        {
            std::vector<PlacementOrigin> result;
            if (cell.has_value())
            {
                result.push_back({"playerSpawnCell", std::nullopt});
            }
            if (feet.has_value())
            {
                result.push_back({"playerSpawnFeet", std::nullopt});
            }
            return result;
        }
    }

    LevelData parseLevelData(std::string_view text, std::string_view sourceName)
    {
        LevelJson file = readContent<LevelJson>(text, sourceName);
        if (file.tileLegend.empty())
        {
            failJson(sourceName, "tileLegend", "expected a nonempty object");
        }
        LevelData result;
        std::vector<ActorPlacementJson> actors =
            file.actors.value_or(std::vector<ActorPlacementJson>{});
        std::vector<PickupPlacementJson> pickups =
            file.pickups.value_or(std::vector<PickupPlacementJson>{});

        if (file.objectLegend.has_value())
        {
            const auto& legend = *file.objectLegend;
            std::vector<std::string> tileSymbols;
            std::vector<std::string> objectSymbols;
            tileSymbols.reserve(file.tileLegend.size());
            objectSymbols.reserve(legend.size());
            for (const auto& entry : file.tileLegend)
            {
                tileSymbols.push_back(entry.first);
            }
            for (const auto& entry : legend)
            {
                objectSymbols.push_back(entry.first);
            }
            validateLegendSymbols(tileSymbols, objectSymbols, sourceName);
            checkLegendTemplates(legend, sourceName, result);
            for (const auto& entry : legend)
            {
                // The marker creates an object, not a terrain tile.
                file.tileLegend[entry.first] = "empty";
            }

            // Turn each marked map cell into a placement, after the written-out ones.
            auto players = playerOrigins(file.playerSpawnCell, file.playerSpawnFeet);
            std::vector<PlacementOrigin> exits;
            if (file.exit.has_value())
            {
                exits.push_back({"exit", std::nullopt});
            }
            for (std::size_t row = 0; row < file.map.size(); ++row)
            {
                const std::string& cells = file.map[row];
                for (std::size_t column = 0; column < cells.size(); ++column)
                {
                    const auto found = legend.find(std::string(1, cells[column]));
                    if (found == legend.end())
                    {
                        continue;
                    }
                    const ObjectTemplateJson& json = found->second;
                    const std::string templatePath = fieldPath("objectLegend", found->first);
                    const Cell cell{static_cast<int>(column), static_cast<int>(row)};
                    const std::string location = indexPath(indexPath("map", row), column);
                    switch (json.type)
                    {
                    case LevelObjectType::Player:
                        players.push_back({location, cells[column]});
                        validateSinglePlacement(players, "player", sourceName);
                        file.playerSpawnCell = cell;
                        break;
                    case LevelObjectType::Exit:
                        exits.push_back({location, cells[column]});
                        validateSinglePlacement(exits, "exit", sourceName);
                        file.exit = exitAt(json, cell, sourceName, templatePath);
                        break;
                    case LevelObjectType::Actor:
                        actors.push_back(actorAt(json, cell, sourceName, templatePath));
                        break;
                    case LevelObjectType::Pickup:
                        pickups.push_back(pickupAt(json, cell));
                        break;
                    }
                }
            }
        }

        std::vector<std::string> tileSymbols;
        tileSymbols.reserve(file.tileLegend.size());
        for (const auto& [symbol, tile] : file.tileLegend)
        {
            tileSymbols.push_back(symbol);
        }
        validateLegendSymbols(tileSymbols, {}, sourceName);
        for (const auto& [symbol, tile] : file.tileLegend)
        {
            result.tileLegend.emplace(symbol.front(), tile);
        }
        if (file.map.empty())
        {
            failJson(sourceName, "map", "expected at least one row");
        }
        result.mapRows = file.map;
        validateMapRows(result.mapRows, result.tileLegend, sourceName);

        validateSinglePlacement(
            playerOrigins(file.playerSpawnCell, file.playerSpawnFeet), "player", sourceName);
        if (!file.exit.has_value())
        {
            failJson(sourceName, "exit", "expected exactly one placement");
        }
        result.playerSpawn = positionFrom(
            file.playerSpawnCell,
            file.playerSpawnFeet,
            "playerSpawnCell",
            "playerSpawnFeet",
            sourceName,
            "root");

        result.actors.reserve(actors.size());
        for (std::size_t index = 0; index < actors.size(); ++index)
        {
            const std::string origin = indexPath("actors", index);
            result.actors.push_back(actorFrom(actors[index], sourceName, origin));
            result.actorReferences.emplace(
                fieldPath(origin, "definition"), result.actors.back().definitionName);
        }

        result.pickups.reserve(pickups.size());
        for (std::size_t index = 0; index < pickups.size(); ++index)
        {
            const std::string origin = indexPath("pickups", index);
            result.pickups.push_back(pickupFrom(pickups[index], sourceName, origin));
            const auto& placement = result.pickups.back();
            if (placement.definitionName.empty())
            {
                result.itemReferences.emplace(fieldPath(origin, "item"), placement.stack.item);
            }
            else
            {
                result.pickupReferences.emplace(
                    fieldPath(origin, "definition"), placement.definitionName);
            }
        }

        result.exit = exitFrom(*file.exit, sourceName, "exit");
        result.exitReferences.emplace("exit.definition", result.exit.definitionName);
        if (result.exit.requirement.has_value())
        {
            result.itemReferences.emplace("exit.requirement.item", result.exit.requirement->item);
        }
        return result;
    }

    LevelData loadLevelData(const std::filesystem::path& path)
    {
        return parseLevelData(loadContentText(path), path.string());
    }
}
