#include "level_data.hpp"

#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "content_validation.hpp"
#include "item_catalog.hpp"

#include <cstddef>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
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
    struct CellPositionJson
    {
        Cell cell;
    };

    struct FeetPositionJson
    {
        glm::vec2 feet;
    };

    using PositionJson = std::variant<CellPositionJson, FeetPositionJson>;

    struct PatrolJson
    {
        PositionJson first;
        PositionJson second;
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
        PositionJson spawn;
        std::optional<PatrolJson> patrol;
    };

    struct PickupPlacementJson
    {
        std::string id;
        std::string definition;
        PositionJson spawn;
    };

    struct ExitPlacementJson
    {
        std::string definition;
        PositionJson spawn;
        std::optional<RequirementJson> requirement;
        std::optional<bool> consumeItem;
        std::optional<int> nextLevel;
    };

    struct LevelJson
    {
        std::map<std::string, std::string> tileLegend;
        std::vector<std::string> map;
        PositionJson playerSpawn;
        std::optional<std::vector<ActorPlacementJson>> actors;
        std::optional<std::vector<PickupPlacementJson>> pickups;
        ExitPlacementJson exit;
    };

    namespace
    {
        LevelPosition positionFrom(const PositionJson& json)
        {
            if (const auto* cell = std::get_if<CellPositionJson>(&json))
            {
                return cell->cell;
            }
            return std::get<FeetPositionJson>(json).feet;
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
            result.spawn = positionFrom(json.spawn);
            if (json.patrol.has_value())
            {
                result.patrol = PatrolPlacement{
                    positionFrom(json.patrol->first), positionFrom(json.patrol->second)};
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
            result.spawn = positionFrom(json.spawn);
            return result;
        }

        ExitPlacement exitFrom(
            const ExitPlacementJson& json,
            std::string_view sourceName,
            const std::string& path)
        {
            ExitPlacement result;
            result.definitionName = json.definition;
            result.spawn = positionFrom(json.spawn);
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

        std::string jsonString(std::string_view text)
        {
            std::string result = "\"";
            for (const char character : text)
            {
                switch (character)
                {
                case '"':
                    result += "\\\"";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                default:
                    if (static_cast<unsigned char>(character) < 0x20U)
                    {
                        result += std::format("\\u{:04x}", static_cast<unsigned>(character));
                    }
                    else
                    {
                        result += character;
                    }
                }
            }
            return result + "\"";
        }

        std::string formatPosition(const LevelPosition& position)
        {
            if (const auto* cell = std::get_if<Cell>(&position))
            {
                return std::format("{{ \"cell\": [{}, {}] }}", cell->x, cell->y);
            }
            const glm::vec2 feet = std::get<glm::vec2>(position);
            return std::format("{{ \"feet\": [{}, {}] }}", feet.x, feet.y);
        }

        std::string formatActor(const ActorPlacement& actor)
        {
            std::string result = std::format(
                "{{ \"id\": {}, \"definition\": {}, \"spawn\": {}",
                jsonString(actor.id),
                jsonString(actor.definitionName),
                formatPosition(actor.spawn));
            if (actor.patrol.has_value())
            {
                result += std::format(
                    ", \"patrol\": {{ \"first\": {}, \"second\": {} }}",
                    formatPosition(actor.patrol->first),
                    formatPosition(actor.patrol->second));
            }
            return result + " }";
        }

        std::string formatPickup(const PickupPlacement& pickup)
        {
            return std::format(
                "{{ \"id\": {}, \"definition\": {}, \"spawn\": {} }}",
                jsonString(pickup.id),
                jsonString(pickup.definitionName),
                formatPosition(pickup.spawn));
        }

        std::string formatExit(const ExitPlacement& exit)
        {
            std::string result = std::format(
                "{{\n    \"definition\": {},\n    \"spawn\": {}",
                jsonString(exit.definitionName),
                formatPosition(exit.spawn));
            if (exit.requirement.has_value())
            {
                result += std::format(
                    ",\n    \"requirement\": {{ \"item\": {}, \"quantity\": {} }}",
                    jsonString(exit.requirement->item),
                    exit.requirement->quantity);
            }
            if (exit.consumeItem)
            {
                result += ",\n    \"consumeItem\": true";
            }
            if (exit.nextLevel.has_value())
            {
                result += std::format(",\n    \"nextLevel\": {}", *exit.nextLevel);
            }
            return result + "\n  }";
        }

        template <class T, class Format>
        std::string formatList(const std::vector<T>& values, Format format)
        {
            std::string result = "[";
            for (std::size_t index = 0; index < values.size(); ++index)
            {
                result += index == 0 ? "\n    " : ",\n    ";
                result += format(values[index]);
            }
            return result + (values.empty() ? "]" : "\n  ]");
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

        result.playerSpawn = positionFrom(file.playerSpawn);

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

        result.exit = exitFrom(file.exit, sourceName, "exit");
        result.exitReferences.emplace("exit.definition", result.exit.definitionName);
        if (result.exit.requirement.has_value())
        {
            result.itemReferences.emplace("exit.requirement.item", result.exit.requirement->item);
        }
        return result;
    }

    std::string formatLevelData(const LevelData& level)
    {
        std::string legend;
        for (const auto& [symbol, tile] : level.tileLegend)
        {
            legend += std::format(
                "{}{}: {}",
                legend.empty() ? "" : ", ",
                jsonString(std::string(1, symbol)),
                jsonString(tile));
        }
        return std::format(
            "{{\n"
            "  \"tileLegend\": {{ {} }},\n"
            "  \"map\": {},\n"
            "  \"playerSpawn\": {},\n"
            "  \"actors\": {},\n"
            "  \"pickups\": {},\n"
            "  \"exit\": {}\n"
            "}}\n",
            legend,
            formatList(level.mapRows, [](const std::string& row) { return jsonString(row); }),
            formatPosition(level.playerSpawn),
            formatList(level.actors, formatActor),
            formatList(level.pickups, formatPickup),
            formatExit(level.exit));
    }
}
