#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/world/sight.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    // Where an actor standing in the cell sees from: the centre of its body.
    glm::vec2 centerOfBodyIn(advanced_platformer::Cell cell)
    {
        return advanced_platformer::centerOf(
            advanced_platformer::boxInCell(tests::TileSize, cell, {12.0F, 12.0F}));
    }
}

TEST_CASE("A wall between two points breaks line of sight", "[world][sight]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({".....", "..w..", "....."})
            .where('w', tests::Tile().blocksMovement().blocksSight());

    REQUIRE_FALSE(
        advanced_platformer::lineOfSight(map, centerOfBodyIn({0, 1}), centerOfBodyIn({4, 1})));
    REQUIRE(advanced_platformer::lineOfSight(map, centerOfBodyIn({0, 2}), centerOfBodyIn({4, 2})));
}

TEST_CASE("A viewer in cover sees out of it but not into other cover", "[world][sight]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({".........", "ccc.....c", "........."})
            .where('c', tests::Tile().blocksSight());

    // The cover the line starts in does not block it, so the viewer sees across its patch
    // and out into the open.
    REQUIRE(advanced_platformer::lineOfSight(map, centerOfBodyIn({0, 1}), centerOfBodyIn({2, 1})));
    REQUIRE(advanced_platformer::lineOfSight(map, centerOfBodyIn({0, 1}), centerOfBodyIn({5, 1})));
    // Entering another patch after a gap does.
    REQUIRE_FALSE(
        advanced_platformer::lineOfSight(map, centerOfBodyIn({0, 1}), centerOfBodyIn({8, 1})));
}
