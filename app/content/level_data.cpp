#include "level_data.hpp"

#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "content_validation.hpp"
#include "item_catalog.hpp"

#include <cstddef>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glaze/glaze.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/math/coordinates.hpp"

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

namespace advanced_platformer
{
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
        std::string id;
        std::string definition;
        std::optional<Cell> spawnCell;
        std::optional<glm::vec2> spawnFeet;
        std::optional<PatrolJson> patrol;
    };

    struct PickupPlacementJson
    {
        std::string id;
        std::string definition;
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

    struct LevelJson
    {
        std::map<std::string, std::string> tileLegend;
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
            result.id = nameFrom(json.id, "placement id", sourceName, fieldPath(path, "id"));
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
            result.id = nameFrom(json.id, "placement id", sourceName, fieldPath(path, "id"));
            result.definitionName = nameFrom(
                json.definition,
                "pickup definition name",
                sourceName,
                fieldPath(path, "definition"));
            result.spawn = positionFrom(
                json.spawnCell, json.spawnFeet, "spawnCell", "spawnFeet", sourceName, path);
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

        void requireUniqueId(
            std::map<std::string, std::string>& seen,
            const std::string& id,
            std::string_view sourceName,
            const std::string& path)
        {
            const auto [existing, inserted] = seen.emplace(id, path);
            if (!inserted)
            {
                failJson(
                    sourceName,
                    fieldPath(path, "id"),
                    std::format("id '{}' is already used by {}", id, existing->second));
            }
        }
    }

    LevelData parseLevelData(std::string_view text, std::string_view sourceName)
    {
        const LevelJson file = readContent<LevelJson>(text, sourceName);
        if (file.tileLegend.empty())
        {
            failJson(sourceName, "tileLegend", "expected a nonempty object");
        }
        LevelData result;

        std::vector<std::string> tileSymbols;
        tileSymbols.reserve(file.tileLegend.size());
        for (const auto& [symbol, tile] : file.tileLegend)
        {
            tileSymbols.push_back(symbol);
        }
        validateLegendSymbols(tileSymbols, sourceName);
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

        std::map<std::string, std::string> ids;
        const std::vector<ActorPlacementJson> actors =
            file.actors.value_or(std::vector<ActorPlacementJson>{});
        result.actors.reserve(actors.size());
        for (std::size_t index = 0; index < actors.size(); ++index)
        {
            const std::string origin = indexPath("actors", index);
            result.actors.push_back(actorFrom(actors[index], sourceName, origin));
            requireUniqueId(ids, result.actors.back().id, sourceName, origin);
            result.actorReferences.emplace(
                fieldPath(origin, "definition"), result.actors.back().definitionName);
        }

        const std::vector<PickupPlacementJson> pickups =
            file.pickups.value_or(std::vector<PickupPlacementJson>{});
        result.pickups.reserve(pickups.size());
        for (std::size_t index = 0; index < pickups.size(); ++index)
        {
            const std::string origin = indexPath("pickups", index);
            result.pickups.push_back(pickupFrom(pickups[index], sourceName, origin));
            requireUniqueId(ids, result.pickups.back().id, sourceName, origin);
            result.pickupReferences.emplace(
                fieldPath(origin, "definition"), result.pickups.back().definitionName);
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
