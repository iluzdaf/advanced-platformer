#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/route.hpp"
#include "advanced_platformer/navigation/platformer_cells.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    using advanced_platformer::Cell;

    constexpr glm::vec2 SmallBody{12.0F, 12.0F};
    constexpr glm::vec2 TallBody{12.0F, 20.0F};
}

TEST_CASE("A standable cell has support below and room for the body", "[navigation][platformer]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({".#.", "...", "###"});

    REQUIRE(advanced_platformer::canStandAt(map, {1, 1}, SmallBody));
    REQUIRE_FALSE(advanced_platformer::canStandAt(map, {1, 1}, TallBody));
    REQUIRE_FALSE(advanced_platformer::canStandAt(map, {1, 0}, SmallBody));
}

TEST_CASE("A body cannot stand or rest in a tile that hurts", "[navigation][platformer]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({".^.", "###"}).where('^', tests::Tile{}.hurts({}));

    REQUIRE(advanced_platformer::canStandAt(map, {0, 0}, SmallBody));
    REQUIRE_FALSE(advanced_platformer::canStandAt(map, {1, 0}, SmallBody));
    REQUIRE_FALSE(advanced_platformer::bodyFits(map, {{16.0F, 4.0F}, SmallBody}));
}

TEST_CASE("A body may extend above the map but not through its walls", "[navigation][platformer]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"....", "####"});
    const glm::vec2 tall{12.0F, 40.0F};
    REQUIRE(advanced_platformer::boxInCell(tests::TileSize, {1, 0}, tall).topLeft.y < 0.0F);
    REQUIRE(advanced_platformer::canStandAt(map, {1, 0}, tall));

    REQUIRE_FALSE(advanced_platformer::canStandAt(map, {0, 0}, {20.0F, 12.0F}));
}

TEST_CASE("A wall location may extend above the open map top", "[navigation][platformer][climb]")
{
    using advanced_platformer::ClimbSurface;
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({".c..", ".c..", "####"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    const glm::vec2 tall{12.0F, 40.0F};
    const advanced_platformer::RouteLocation onWall{{2, 0}, ClimbSurface::LeftWall};

    REQUIRE(advanced_platformer::boundsAtSurface(tests::TileSize, onWall, tall).topLeft.y < 0.0F);
    REQUIRE(advanced_platformer::canOccupy(map, onWall, tall));
}

TEST_CASE(
    "A climbable cell has a climbable wall or ceiling the body can rest on",
    "[navigation][platformer][climb]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"ccccc", "c...#", "c...#", "#####"})
            .where('c', tests::Tile{}.blocksMovement().climbable());

    REQUIRE(advanced_platformer::canClimbAt(map, {1, 2}, SmallBody));
    REQUIRE(advanced_platformer::canClimbAt(map, {2, 1}, SmallBody));
    REQUIRE_FALSE(advanced_platformer::canClimbAt(map, {2, 2}, SmallBody));
    REQUIRE_FALSE(advanced_platformer::canClimbAt(map, {3, 2}, SmallBody));
    REQUIRE_FALSE(advanced_platformer::canClimbAt(map, {0, 2}, SmallBody));
}
