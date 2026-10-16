#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/render/sprite.hpp"

namespace advanced_platformer
{
    struct TouchDamage
    {
        int damage = 1;
        float lift = 0.0F;
        float repeatSeconds = 1.0F;
        float height = 16.0F;

        bool operator==(const TouchDamage&) const = default;
    };

    struct TileDefinition
    {
        bool blocksMovement = false;
        bool blocksSight = false;
        SpriteRegion sprite;
        std::optional<int> breaksIntoTileId = std::nullopt;
        bool climbable = false;
        std::optional<TouchDamage> touchDamage = std::nullopt;
    };

    class TileMap
    {
    public:
        TileMap(
            int tileSize,
            int width,
            int height,
            std::vector<int> tiles,
            std::vector<TileDefinition> definitions);

        static TileMap fromAscii(
            int tileSize,
            const std::vector<std::string>& rows,
            std::vector<TileDefinition> definitions,
            const std::map<char, int>& legend);

        int tileSize() const;
        int width() const;
        int height() const;
        GridSize size() const;
        float pixelWidth() const;
        float pixelHeight() const;

        bool contains(Cell cell) const;
        int tileAt(Cell cell) const;
        const TileDefinition& definitionAt(Cell cell) const;

        bool blocksMovement(Cell cell) const;
        bool blocksSight(Cell cell) const;
        bool climbableAt(Cell cell) const;
        bool hurtsAt(Cell cell) const;

        bool breakTile(Cell cell);
        const std::vector<Cell>& brokenCells() const;

    private:
        std::size_t indexOf(Cell cell) const;

        int mapTileSize = 0;
        int mapWidth = 0;
        int mapHeight = 0;
        std::vector<int> tileIds;
        std::vector<TileDefinition> tileDefinitions;
        std::vector<Cell> brokenCellLog;
    };
}
