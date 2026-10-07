#include "room_pieces.hpp"

#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "content_validation.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <string_view>
#include <utility>
#include <vector>

#include <glaze/glaze.hpp>

#include "advanced_platformer/level/placements.hpp"
#include "advanced_platformer/level/room_pieces.hpp"
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

template <> struct glz::meta<advanced_platformer::RoomSide>
{
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr std::array keys{"left", "right", "up", "down"};
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr std::array value{
        advanced_platformer::RoomSide::Left,
        advanced_platformer::RoomSide::Right,
        advanced_platformer::RoomSide::Up,
        advanced_platformer::RoomSide::Down};
};

template <> struct glz::meta<advanced_platformer::RoomRole>
{
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr std::array keys{"start", "exit", "corridor", "shaft", "arena"};
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr std::array value{
        advanced_platformer::RoomRole::Start,
        advanced_platformer::RoomRole::Exit,
        advanced_platformer::RoomRole::Corridor,
        advanced_platformer::RoomRole::Shaft,
        advanced_platformer::RoomRole::Arena};
};

namespace advanced_platformer
{
    struct PatrolJson
    {
        Cell first;
        Cell second;
    };

    struct RequirementJson
    {
        std::string item;
        int quantity = 1;
    };

    struct ActorPlacementJson
    {
        std::string id;
        std::string definition;
        Cell spawn;
        std::optional<PatrolJson> patrol;
    };

    struct PickupPlacementJson
    {
        std::string id;
        std::string definition;
        Cell spawn;
    };

    struct ExitPlacementJson
    {
        std::string definition;
        Cell spawn;
        std::optional<RequirementJson> requirement;
        std::optional<bool> consumeItem;
    };

    struct RoomPieceJson
    {
        RoomRole role = RoomRole::Corridor;
        std::vector<RoomSide> doors;
        std::optional<bool> mirror;
        std::vector<std::string> map;
        std::optional<Cell> playerSpawn;
        std::optional<ExitPlacementJson> exit;
        std::optional<std::vector<ActorPlacementJson>> actors;
        std::optional<std::vector<PickupPlacementJson>> pickups;
    };

    struct RunJson
    {
        std::optional<std::array<int, 2>> grid;
        int firstRooms = 0;
        int roomsPerLevel = 0;
        int maxRooms = 0;
    };

    struct RoomPieceCatalogJson
    {
        std::array<int, 2> roomSize{};
        RunJson run;
        std::map<std::string, std::string> tileLegend;
    };

    namespace
    {
        constexpr int MinimumRoomWidth = 8;
        constexpr int MinimumRoomHeight = 6;

        constexpr std::array AllSides{
            RoomSide::Left,
            RoomSide::Right,
            RoomSide::Up,
            RoomSide::Down};

        std::string roleName(RoomRole role)
        {
            switch (role)
            {
            case RoomRole::Start:
                return "start";
            case RoomRole::Exit:
                return "exit";
            case RoomRole::Corridor:
                return "corridor";
            case RoomRole::Shaft:
                return "shaft";
            case RoomRole::Arena:
                return "arena";
            }
            return "unknown";
        }

        constexpr int SideDoorHeight = 3;
        constexpr int VerticalDoorWidth = 4;

        std::vector<Cell> doorCells(GridSize roomSize, RoomSide side)
        {
            std::vector<Cell> cells;
            const int floor = roomSize.height - 1;
            const int firstColumn = (roomSize.width / 2) - (VerticalDoorWidth / 2);
            switch (side)
            {
            case RoomSide::Left:
            case RoomSide::Right: {
                const int column = side == RoomSide::Left ? 0 : roomSize.width - 1;
                for (int row = floor - SideDoorHeight; row < floor; ++row)
                {
                    cells.push_back({column, row});
                }
                break;
            }
            case RoomSide::Up:
            case RoomSide::Down: {
                const int row = side == RoomSide::Up ? 0 : floor;
                for (int column = firstColumn; column < firstColumn + VerticalDoorWidth; ++column)
                {
                    cells.push_back({column, row});
                }
                break;
            }
            }
            return cells;
        }

        bool onDoor(GridSize roomSize, RoomDoors doors, Cell cell)
        {
            return std::ranges::any_of(
                AllSides,
                [&](RoomSide side)
                {
                    return hasDoor(doors, side) &&
                           std::ranges::contains(doorCells(roomSize, side), cell);
                });
        }

        bool onEdge(GridSize roomSize, Cell cell)
        {
            return cell.x == 0 || cell.y == 0 || cell.x == roomSize.width - 1 ||
                   cell.y == roomSize.height - 1;
        }

        void validatePieceMap(
            const RoomPiece& piece,
            const RoomPieceCatalog& catalog,
            std::string_view sourceName,
            const std::string& path)
        {
            const GridSize size = catalog.roomSize;
            if (std::cmp_not_equal(piece.rows.size(), size.height))
            {
                failJson(
                    sourceName,
                    path,
                    std::format("expected {} rows, got {}", size.height, piece.rows.size()));
            }
            for (int row = 0; row < size.height; ++row)
            {
                const std::string& text = piece.rows[static_cast<std::size_t>(row)];
                const std::string rowPath = indexPath(path, static_cast<std::size_t>(row));
                if (std::cmp_not_equal(text.size(), size.width))
                {
                    failJson(
                        sourceName,
                        rowPath,
                        std::format("expected {} columns, got {}", size.width, text.size()));
                }
                for (int column = 0; column < size.width; ++column)
                {
                    const char symbol = text[static_cast<std::size_t>(column)];
                    const Cell cell{column, row};
                    const std::string cellPath =
                        indexPath(rowPath, static_cast<std::size_t>(column));
                    if (!catalog.tileLegend.contains(symbol))
                    {
                        failJson(
                            sourceName,
                            cellPath,
                            std::format("unknown symbol '{}'; define it in tileLegend", symbol));
                    }
                    if (onEdge(size, cell))
                    {
                        const bool door = onDoor(size, piece.doors, cell);
                        if (door && symbol != catalog.open)
                        {
                            failJson(
                                sourceName,
                                cellPath,
                                std::format(
                                    "expected '{}': an edge is open on a door", catalog.open));
                        }
                        if (!door && symbol == catalog.open)
                        {
                            failJson(
                                sourceName,
                                cellPath,
                                std::format(
                                    "expected a tile, not '{}': an edge is solid away from the "
                                    "doors",
                                    catalog.open));
                        }
                    }
                }
            }
        }

        Cell cellFrom(
            Cell cell,
            GridSize size,
            std::string_view sourceName,
            const std::string& path)
        {
            if (cell.x < 0 || cell.y < 0 || cell.x >= size.width || cell.y >= size.height)
            {
                failJson(
                    sourceName,
                    path,
                    std::format(
                        "expected a cell inside the {} by {} piece", size.width, size.height));
            }
            return cell;
        }

        std::string nameFrom(
            const std::string& name,
            std::string_view description,
            std::string_view sourceName,
            const std::string& path)
        {
            if (name.empty())
            {
                failJson(sourceName, path, std::format("{} cannot be empty", description));
            }
            return name;
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

        ExitPlacement exitFrom(
            const ExitPlacementJson& json,
            GridSize size,
            std::string_view sourceName,
            const std::string& path)
        {
            ExitPlacement exit;
            exit.definitionName = nameFrom(
                json.definition, "exit definition name", sourceName, fieldPath(path, "definition"));
            exit.spawn = cellFrom(json.spawn, size, sourceName, fieldPath(path, "spawn"));
            if (json.requirement.has_value())
            {
                const std::string requirementPath = fieldPath(path, "requirement");
                if (json.requirement->quantity <= 0)
                {
                    failJson(
                        sourceName,
                        fieldPath(requirementPath, "quantity"),
                        std::format(
                            "expected a positive integer, got {}", json.requirement->quantity));
                }
                exit.requirement = NamedItemStack{
                    nameFrom(
                        json.requirement->item,
                        "item name",
                        sourceName,
                        fieldPath(requirementPath, "item")),
                    json.requirement->quantity};
            }
            exit.consumeItem = json.consumeItem.value_or(false);
            return exit;
        }

        void placementsFrom(
            const RoomPieceJson& json,
            RoomPiece& piece,
            GridSize size,
            std::string_view sourceName,
            const std::string& path)
        {
            if (json.playerSpawn.has_value() != (piece.role == RoomRole::Start))
            {
                failJson(
                    sourceName,
                    fieldPath(path, "playerSpawn"),
                    std::format(
                        "{} rooms {} a player spawn",
                        roleName(piece.role),
                        piece.role == RoomRole::Start ? "need" : "cannot have"));
            }
            if (json.exit.has_value() != (piece.role == RoomRole::Exit))
            {
                failJson(
                    sourceName,
                    fieldPath(path, "exit"),
                    std::format(
                        "{} rooms {} an exit",
                        roleName(piece.role),
                        piece.role == RoomRole::Exit ? "need" : "cannot have"));
            }
            if (json.playerSpawn.has_value())
            {
                piece.playerSpawn =
                    cellFrom(*json.playerSpawn, size, sourceName, fieldPath(path, "playerSpawn"));
            }
            if (json.exit.has_value())
            {
                piece.exit = exitFrom(*json.exit, size, sourceName, fieldPath(path, "exit"));
            }
            std::map<std::string, std::string> ids;
            const std::vector<ActorPlacementJson> actors =
                json.actors.value_or(std::vector<ActorPlacementJson>{});
            for (std::size_t index = 0; index < actors.size(); ++index)
            {
                const std::string origin = indexPath(fieldPath(path, "actors"), index);
                const ActorPlacementJson& placement = actors[index];
                ActorPlacement actor;
                actor.id =
                    nameFrom(placement.id, "placement id", sourceName, fieldPath(origin, "id"));
                actor.definitionName = nameFrom(
                    placement.definition,
                    "actor definition name",
                    sourceName,
                    fieldPath(origin, "definition"));
                actor.spawn =
                    cellFrom(placement.spawn, size, sourceName, fieldPath(origin, "spawn"));
                if (placement.patrol.has_value())
                {
                    const std::string patrolPath = fieldPath(origin, "patrol");
                    const PatrolJson patrol = placement.patrol.value_or(PatrolJson{});
                    actor.patrol = PatrolPlacement{
                        cellFrom(patrol.first, size, sourceName, fieldPath(patrolPath, "first")),
                        cellFrom(patrol.second, size, sourceName, fieldPath(patrolPath, "second"))};
                }
                requireUniqueId(ids, actor.id, sourceName, origin);
                piece.actors.push_back(std::move(actor));
            }
            const std::vector<PickupPlacementJson> pickups =
                json.pickups.value_or(std::vector<PickupPlacementJson>{});
            for (std::size_t index = 0; index < pickups.size(); ++index)
            {
                const std::string origin = indexPath(fieldPath(path, "pickups"), index);
                PickupPlacement pickup;
                pickup.id = nameFrom(
                    pickups[index].id, "placement id", sourceName, fieldPath(origin, "id"));
                pickup.definitionName = nameFrom(
                    pickups[index].definition,
                    "pickup definition name",
                    sourceName,
                    fieldPath(origin, "definition"));
                pickup.spawn =
                    cellFrom(pickups[index].spawn, size, sourceName, fieldPath(origin, "spawn"));
                requireUniqueId(ids, pickup.id, sourceName, origin);
                piece.pickups.push_back(std::move(pickup));
            }
        }

        struct RoomPieceSource
        {
            std::string name;
            std::string text;
            std::string sourceName;
        };

        int doorCount(RoomDoors doors)
        {
            return std::popcount(doors.bits);
        }

        RoomPiece pieceFrom(const RoomPieceSource& source, GridSize size)
        {
            const auto json = readContent<RoomPieceJson>(source.text, source.sourceName);
            const std::string_view sourceName = source.sourceName;
            const std::string path;
            RoomPiece piece;
            piece.name = source.name;
            piece.role = json.role;
            for (const RoomSide side : json.doors)
            {
                if (hasDoor(piece.doors, side))
                {
                    failJson(
                        sourceName,
                        fieldPath(path, "doors"),
                        std::format("door {} is listed twice", nameOf(side)));
                }
                piece.doors = withDoor(piece.doors, side);
            }
            if (doorCount(piece.doors) == 0)
            {
                failJson(sourceName, fieldPath(path, "doors"), "expected at least one door");
            }
            piece.mirror = json.mirror.value_or(true);
            piece.rows = json.map;
            placementsFrom(json, piece, size, sourceName, path);
            return piece;
        }

        constexpr std::array<int, 2> DefaultGenerationGrid = {9, 7};

        RunSettings runFrom(const RunJson& json, std::string_view sourceName)
        {
            RunSettings run;
            const std::array<int, 2> grid = json.grid.value_or(DefaultGenerationGrid);
            run.grid = {grid[0], grid[1]};
            if (run.grid.width <= 0 || run.grid.height <= 0)
            {
                failJson(sourceName, "run.grid", "expected a positive size");
            }
            const int slots = run.grid.width * run.grid.height;
            run.firstRooms = json.firstRooms;
            run.roomsPerLevel = json.roomsPerLevel;
            run.maxRooms = json.maxRooms;
            if (run.maxRooms < 2 || run.maxRooms > slots)
            {
                failJson(
                    sourceName,
                    "run.maxRooms",
                    std::format(
                        "expected at least 2 rooms and no more than the grid's {} slots", slots));
            }
            if (run.firstRooms < 2 || run.firstRooms > run.maxRooms)
            {
                failJson(
                    sourceName,
                    "run.firstRooms",
                    "expected at least 2 rooms and no more than maxRooms");
            }
            if (run.roomsPerLevel < 0)
            {
                failJson(sourceName, "run.roomsPerLevel", "expected zero or more rooms");
            }
            return run;
        }

        RoomPieceCatalog parseRoomPieceCatalog(
            std::string_view text,
            std::string_view sourceName,
            std::span<const RoomPieceSource> pieces)
        {
            const auto file = readContent<RoomPieceCatalogJson>(text, sourceName);
            RoomPieceCatalog result;
            result.roomSize = {file.roomSize[0], file.roomSize[1]};
            result.run = runFrom(file.run, sourceName);
            if (result.roomSize.width < MinimumRoomWidth ||
                result.roomSize.height < MinimumRoomHeight || result.roomSize.width % 2 != 0)
            {
                failJson(
                    sourceName,
                    "roomSize",
                    std::format(
                        "expected an even width of at least {} and a height of at least {}",
                        MinimumRoomWidth,
                        MinimumRoomHeight));
            }

            if (file.tileLegend.empty())
            {
                failJson(sourceName, "tileLegend", "expected a nonempty object");
            }
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
            const auto open = std::ranges::find_if(
                result.tileLegend, [](const auto& entry) { return entry.second == "empty"; });
            if (open == result.tileLegend.end())
            {
                failJson(sourceName, "tileLegend", "expected a symbol for empty");
            }
            result.open = open->first;

            for (const RoomPieceSource& source : pieces)
            {
                RoomPiece piece = pieceFrom(source, result.roomSize);
                validatePieceMap(piece, result, source.sourceName, "map");
                result.pieces.push_back(std::move(piece));
            }
            return result;
        }
    }

    RoomPieceCatalog loadRoomPieceCatalog(const std::filesystem::path& path)
    {
        const std::filesystem::path folder = path.parent_path() / "pieces";
        std::vector<std::filesystem::path> files;
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(folder, error))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                files.push_back(entry.path());
            }
        }
        if (files.empty())
        {
            failJson(path.string(), "", std::format("expected piece files in {}", folder.string()));
        }
        std::ranges::sort(files);
        std::vector<RoomPieceSource> pieces;
        pieces.reserve(files.size());
        for (const std::filesystem::path& file : files)
        {
            pieces.push_back({file.stem().string(), loadContentText(file), file.string()});
        }
        return parseRoomPieceCatalog(loadContentText(path), path.string(), pieces);
    }
}
