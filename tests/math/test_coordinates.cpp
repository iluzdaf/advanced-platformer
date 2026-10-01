#include <catch2/catch_test_macros.hpp>

#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "support/require_near.hpp"

namespace
{
    using advanced_platformer::Aabb;

}

TEST_CASE("An AABB position is its top-left corner", "[math][coordinates]")
{
    const Aabb box{{10.0F, 20.0F}, {12.0F, 24.0F}};

    REQUIRE_NEAR(advanced_platformer::centerOf(box).x, 16.0F);

    REQUIRE_NEAR(advanced_platformer::centerOf(box).y, 32.0F);
    REQUIRE_NEAR(advanced_platformer::feetOf(box).x, 16.0F);
    REQUIRE_NEAR(advanced_platformer::feetOf(box).y, 44.0F);
    REQUIRE_NEAR(advanced_platformer::topCenterOf(box).x, 16.0F);
    REQUIRE_NEAR(advanced_platformer::topCenterOf(box).y, 20.0F);
}

TEST_CASE("An AABB's far edges are its position plus its size", "[math][aabb]")
{
    const Aabb box{{10.0F, 20.0F}, {12.0F, 24.0F}};

    REQUIRE_NEAR(advanced_platformer::rightOf(box), 22.0F);
    REQUIRE_NEAR(advanced_platformer::bottomOf(box), 44.0F);
}

TEST_CASE("Moving an AABB by its feet keeps its size", "[math][aabb]")
{
    Aabb box{{0.0F, 0.0F}, {12.0F, 24.0F}};

    advanced_platformer::moveFeetTo(box, {40.0F, 128.0F});

    REQUIRE_NEAR(box.topLeft.x, 34.0F);
    REQUIRE_NEAR(box.topLeft.y, 104.0F);
    REQUIRE(box.size == glm::vec2{12.0F, 24.0F});
}

TEST_CASE("An AABB can be built around its centre", "[math][aabb]")
{
    const Aabb box = advanced_platformer::boxCenteredOn({16.0F, 32.0F}, {12.0F, 24.0F});

    REQUIRE_NEAR(box.topLeft.x, 10.0F);
    REQUIRE_NEAR(box.topLeft.y, 20.0F);
    REQUIRE(box.size == glm::vec2{12.0F, 24.0F});
    REQUIRE_NEAR(advanced_platformer::centerOf(box).x, 16.0F);
    REQUIRE_NEAR(advanced_platformer::centerOf(box).y, 32.0F);
}

TEST_CASE("An arbitrary-sized AABB can be built standing on its feet", "[math][coordinates]")
{
    const Aabb box = advanced_platformer::boxStandingOn({40.0F, 128.0F}, {12.0F, 24.0F});

    REQUIRE_NEAR(box.topLeft.x, 34.0F);
    REQUIRE_NEAR(box.topLeft.y, 104.0F);
    REQUIRE(box.size == glm::vec2{12.0F, 24.0F});
    REQUIRE_NEAR(advanced_platformer::feetOf(box).x, 40.0F);
    REQUIRE_NEAR(advanced_platformer::feetOf(box).y, 128.0F);
}

TEST_CASE("AABBs overlap when their areas intersect", "[math][aabb]")
{
    const Aabb first{{10.0F, 10.0F}, {10.0F, 10.0F}};
    const Aabb second{{15.0F, 15.0F}, {10.0F, 10.0F}};

    REQUIRE(advanced_platformer::overlaps(first, second));
    REQUIRE(advanced_platformer::overlaps(second, first));
}

TEST_CASE("AABBs that only touch at an edge do not overlap", "[math][aabb]")
{
    const Aabb first{{10.0F, 10.0F}, {10.0F, 10.0F}};
    const Aabb second{{20.0F, 10.0F}, {10.0F, 10.0F}};

    REQUIRE_FALSE(advanced_platformer::overlaps(first, second));
}

TEST_CASE("An AABB contains points inside it and on its near edges", "[math][aabb]")
{
    const advanced_platformer::Aabb box{{10.0F, 20.0F}, {4.0F, 6.0F}};

    REQUIRE(advanced_platformer::contains(box, {12.0F, 23.0F}));
    REQUIRE(advanced_platformer::contains(box, {10.0F, 20.0F}));
    REQUIRE_FALSE(advanced_platformer::contains(box, {14.0F, 23.0F}));
    REQUIRE_FALSE(advanced_platformer::contains(box, {12.0F, 26.0F}));
    REQUIRE_FALSE(advanced_platformer::contains(box, {9.0F, 23.0F}));
}

TEST_CASE("Cells compare by both coordinates", "[math][coordinates]")
{
    using advanced_platformer::Cell;

    REQUIRE(Cell{2, 3} == Cell{2, 3});
    REQUIRE(Cell{2, 3} != Cell{3, 2});
}

TEST_CASE("World points and cells convert at tile boundaries", "[math][coordinates]")
{
    using advanced_platformer::Cell;

    REQUIRE(advanced_platformer::cellAt(16, {0.0F, 0.0F}) == Cell{0, 0});
    REQUIRE(advanced_platformer::cellAt(16, {15.9F, 31.9F}) == Cell{0, 1});
    REQUIRE(advanced_platformer::cellAt(16, {16.0F, 32.0F}) == Cell{1, 2});
    REQUIRE(advanced_platformer::cellAt(16, {-0.1F, -16.1F}) == Cell{-1, -2});
    REQUIRE_NEAR(advanced_platformer::cellCorner(16, {2, 3}).x, 32.0F);
    REQUIRE_NEAR(advanced_platformer::cellCorner(16, {2, 3}).y, 48.0F);
}

TEST_CASE("The tile size scales every cell conversion", "[math][coordinates]")
{
    using advanced_platformer::Cell;

    REQUIRE(advanced_platformer::cellAt(32, {31.9F, 32.0F}) == Cell{0, 1});
    REQUIRE(advanced_platformer::cellCorner(32, {2, 3}) == glm::vec2{64.0F, 96.0F});
    REQUIRE(advanced_platformer::cellAtFeet(32, {48.0F, 64.0F}) == Cell{1, 1});
    REQUIRE(advanced_platformer::feetInCell(32, {1, 1}) == glm::vec2{48.0F, 64.0F});
    REQUIRE(
        advanced_platformer::boxInCell(32, {1, 1}, {12.0F, 20.0F}).topLeft ==
        glm::vec2{42.0F, 44.0F});
}

TEST_CASE("Cells and feet convert on tile boundaries", "[math][coordinates]")
{
    REQUIRE(advanced_platformer::cellAtFeet(16, {24.0F, 32.0F}) == advanced_platformer::Cell{1, 1});
    REQUIRE(advanced_platformer::feetInCell(16, {1, 1}) == glm::vec2{24.0F, 32.0F});
}

TEST_CASE("A box covers the cells inside its edges, not the ones it only touches", "[math][aabb]")
{
    using advanced_platformer::Cell;

    // Every edge on a boundary: exactly the one cell.
    const auto onBoundaries =
        advanced_platformer::cellsCovered(16, {{16.0F, 16.0F}, {16.0F, 16.0F}});
    REQUIRE(onBoundaries.first == Cell{1, 1});
    REQUIRE(onBoundaries.last == Cell{1, 1});

    const auto straddling = advanced_platformer::cellsCovered(16, {{8.0F, 8.0F}, {16.0F, 16.0F}});
    REQUIRE(straddling.first == Cell{0, 0});
    REQUIRE(straddling.last == Cell{1, 1});

    // A 12 by 20 body standing in a cell reaches into the cell above, and no further sideways.
    const auto standing = advanced_platformer::cellsCovered(
        16, advanced_platformer::boxInCell(16, {1, 1}, {12.0F, 20.0F}));
    REQUIRE(standing.first == Cell{1, 0});
    REQUIRE(standing.last == Cell{1, 1});
}
