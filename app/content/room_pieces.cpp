#include "room_pieces.hpp"

#include "content_diagnostics.hpp"
#include "content_glaze.hpp"
#include "content_validation.hpp"
#include "item_catalog.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glaze/glaze.hpp>

#include "advanced_platformer/math/coordinates.hpp"

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
    struct RoomMarkersJson
    {
        std::string start;
        std::string exit;
        std::optional<std::map<std::string, std::string>> actors;
        std::optional<std::map<std::string, std::string>> pickups;
    };

    struct RoomExitRequirementJson
    {
        std::string item;
        int quantity = 1;
    };

    struct RoomExitJson
    {
        std::string definition;
        std::optional<RoomExitRequirementJson> requirement;
        std::optional<bool> consumeItem;
    };

    struct RoomPieceJson
    {
        std::string name;
        RoomRole role = RoomRole::Corridor;
        std::vector<RoomSide> doors;
        std::optional<bool> mirror;
        std::vector<std::string> map;
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
        std::string wall;
        std::string open;
        RoomMarkersJson markers;
        RoomExitJson exit;
        std::vector<RoomPieceJson> pieces;
    };

    namespace
    {
        std::uint8_t bitOf(RoomSide side)
        {
            return static_cast<std::uint8_t>(1U << static_cast<unsigned>(side));
        }
    }

    bool hasDoor(RoomDoors doors, RoomSide side)
    {
        return (doors.bits & bitOf(side)) != 0U;
    }

    RoomDoors withDoor(RoomDoors doors, RoomSide side)
    {
        return {static_cast<std::uint8_t>(doors.bits | bitOf(side))};
    }

    bool coversDoors(RoomDoors doors, RoomDoors other)
    {
        return (doors.bits & other.bits) == other.bits;
    }

    int doorCount(RoomDoors doors)
    {
        return std::popcount(doors.bits);
    }

    RoomDoors mirroredDoors(RoomDoors doors)
    {
        RoomDoors result{static_cast<std::uint8_t>(
            doors.bits & static_cast<std::uint8_t>(bitOf(RoomSide::Up) | bitOf(RoomSide::Down)))};
        if (hasDoor(doors, RoomSide::Left))
        {
            result = withDoor(result, RoomSide::Right);
        }
        if (hasDoor(doors, RoomSide::Right))
        {
            result = withDoor(result, RoomSide::Left);
        }
        return result;
    }

    RoomSide oppositeOf(RoomSide side)
    {
        switch (side)
        {
        case RoomSide::Left:
            return RoomSide::Right;
        case RoomSide::Right:
            return RoomSide::Left;
        case RoomSide::Up:
            return RoomSide::Down;
        case RoomSide::Down:
            return RoomSide::Up;
        }
        throw std::invalid_argument("Unknown room side");
    }

    Cell stepTowards(Cell cell, RoomSide side)
    {
        switch (side)
        {
        case RoomSide::Left:
            return {cell.x - 1, cell.y};
        case RoomSide::Right:
            return {cell.x + 1, cell.y};
        case RoomSide::Up:
            return {cell.x, cell.y - 1};
        case RoomSide::Down:
            return {cell.x, cell.y + 1};
        }
        throw std::invalid_argument("Unknown room side");
    }

    std::string_view nameOf(RoomSide side)
    {
        switch (side)
        {
        case RoomSide::Left:
            return "left";
        case RoomSide::Right:
            return "right";
        case RoomSide::Up:
            return "up";
        case RoomSide::Down:
            return "down";
        }
        return "unknown";
    }

    namespace
    {
        constexpr int SideDoorHeight = 3;
        constexpr int VerticalDoorWidth = 4;
    }

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

    namespace
    {
        constexpr int MinimumRoomWidth = 8;
        constexpr int MinimumRoomHeight = 6;

        constexpr std::array AllSides{
            RoomSide::Left,
            RoomSide::Right,
            RoomSide::Up,
            RoomSide::Down};

        char symbolFrom(std::string_view text, std::string_view sourceName, std::string_view path)
        {
            if (text.size() != 1)
            {
                failJson(sourceName, path, "expected one character");
            }
            return text.front();
        }

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

        struct MarkerCounts
        {
            int starts = 0;
            int exits = 0;
        };

        MarkerCounts validatePieceMap(
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
            MarkerCounts counts;
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
                    const bool tile = catalog.tileLegend.contains(symbol);
                    if (!tile && symbol != catalog.startMarker && symbol != catalog.exitMarker &&
                        !catalog.actorMarkers.contains(symbol) &&
                        !catalog.pickupMarkers.contains(symbol))
                    {
                        failJson(
                            sourceName,
                            cellPath,
                            std::format(
                                "unknown symbol '{}'; define it in tileLegend or markers", symbol));
                    }
                    if (onEdge(size, cell))
                    {
                        const bool door = onDoor(size, piece.doors, cell);
                        const char expected = door ? catalog.open : catalog.wall;
                        if (symbol != expected)
                        {
                            failJson(
                                sourceName,
                                cellPath,
                                std::format(
                                    "expected '{}': an edge is {}",
                                    expected,
                                    door ? "open on a door" : "wall away from the doors"));
                        }
                    }
                    counts.starts += symbol == catalog.startMarker ? 1 : 0;
                    counts.exits += symbol == catalog.exitMarker ? 1 : 0;
                }
            }
            return counts;
        }

        void validateRoleMarkers(
            const RoomPiece& piece,
            MarkerCounts counts,
            std::string_view sourceName,
            const std::string& path)
        {
            const int starts = piece.role == RoomRole::Start ? 1 : 0;
            const int exits = piece.role == RoomRole::Exit ? 1 : 0;
            if (counts.starts != starts || counts.exits != exits)
            {
                failJson(
                    sourceName,
                    path,
                    std::format(
                        "a {} room has {} start and {} exit markers, not {} and {}",
                        roleName(piece.role),
                        starts,
                        exits,
                        counts.starts,
                        counts.exits));
            }
        }

        std::map<char, std::string> markerMap(
            const std::optional<std::map<std::string, std::string>>& json,
            std::set<char>& used,
            std::string_view sourceName,
            const std::string& path)
        {
            std::map<char, std::string> result;
            for (const auto& [symbol, definition] :
                 json.value_or(std::map<std::string, std::string>{}))
            {
                const std::string symbolPath = fieldPath(path, symbol);
                const char marker = symbolFrom(symbol, sourceName, symbolPath);
                if (!used.insert(marker).second)
                {
                    failJson(sourceName, symbolPath, "symbol is already a tile or marker");
                }
                if (definition.empty())
                {
                    failJson(sourceName, symbolPath, "definition name cannot be empty");
                }
                result.emplace(marker, definition);
            }
            return result;
        }

        RoomPiece pieceFrom(
            const RoomPieceJson& json,
            std::string_view sourceName,
            const std::string& path)
        {
            RoomPiece piece;
            if (json.name.empty())
            {
                failJson(sourceName, fieldPath(path, "name"), "piece name cannot be empty");
            }
            piece.name = json.name;
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
    }

    RoomPieceCatalog parseRoomPieceCatalog(std::string_view text, std::string_view sourceName)
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
        std::set<char> used;
        for (const auto& [symbol, tile] : file.tileLegend)
        {
            result.tileLegend.emplace(symbol.front(), tile);
            used.insert(symbol.front());
        }
        result.wall = symbolFrom(file.wall, sourceName, "wall");
        result.open = symbolFrom(file.open, sourceName, "open");
        for (const auto& [symbol, path] :
             {std::pair{result.wall, "wall"}, std::pair{result.open, "open"}})
        {
            if (!result.tileLegend.contains(symbol))
            {
                failJson(sourceName, path, "symbol is not in tileLegend");
            }
        }

        result.startMarker = symbolFrom(file.markers.start, sourceName, "markers.start");
        result.exitMarker = symbolFrom(file.markers.exit, sourceName, "markers.exit");
        for (const auto& [symbol, path] :
             {std::pair{result.startMarker, "markers.start"},
              std::pair{result.exitMarker, "markers.exit"}})
        {
            if (!used.insert(symbol).second)
            {
                failJson(sourceName, path, "symbol is already a tile or marker");
            }
        }
        result.actorMarkers = markerMap(file.markers.actors, used, sourceName, "markers.actors");
        result.pickupMarkers = markerMap(file.markers.pickups, used, sourceName, "markers.pickups");

        if (file.exit.definition.empty())
        {
            failJson(sourceName, "exit.definition", "exit definition name cannot be empty");
        }
        result.exitDefinition = file.exit.definition;
        if (file.exit.requirement.has_value())
        {
            result.exitRequirement =
                NamedItemStack{file.exit.requirement->item, file.exit.requirement->quantity};
        }
        result.consumeExitItem = file.exit.consumeItem.value_or(false);

        std::set<std::string> names;
        for (std::size_t index = 0; index < file.pieces.size(); ++index)
        {
            const std::string path = indexPath("pieces", index);
            RoomPiece piece = pieceFrom(file.pieces[index], sourceName, path);
            if (!names.insert(piece.name).second)
            {
                failJson(
                    sourceName,
                    fieldPath(path, "name"),
                    std::format("piece name '{}' is already used", piece.name));
            }
            const MarkerCounts counts =
                validatePieceMap(piece, result, sourceName, fieldPath(path, "map"));
            validateRoleMarkers(piece, counts, sourceName, path);
            result.pieces.push_back(std::move(piece));
        }
        return result;
    }

    RoomPieceCatalog loadRoomPieceCatalog(const std::filesystem::path& path)
    {
        return parseRoomPieceCatalog(loadContentText(path), path.string());
    }
}
