#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <limits>
#include <stdexcept>

#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/movement/flying_movement.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/physics/body.hpp"
#include "advanced_platformer/physics/collision.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "support/tile_map_builder.hpp"

TEST_CASE("Flying movement normalizes two-dimensional intentions", "[movement][flying]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({".....", ".....", ".....", ".....", "#####"});
    advanced_platformer::Body body{{{16.0F, 16.0F}, {8.0F, 8.0F}}, {0.0F, 0.0F}};
    const advanced_platformer::FlyingMovement movement{10.0F};
    advanced_platformer::InputIntentions intentions;
    intentions.direction = {1.0F, 1.0F};

    advanced_platformer::updateFlyingMovement(map, body, movement, intentions, 1.0F);

    REQUIRE_THAT(body.velocity.x, Catch::Matchers::WithinAbs(7.071F, 0.001F));
    REQUIRE_THAT(body.velocity.y, Catch::Matchers::WithinAbs(7.071F, 0.001F));
}

TEST_CASE("Flying movement uses tile collision", "[movement][flying]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({".....", "..#..", ".....", ".....", "#####"});
    advanced_platformer::Body body{{{16.0F, 16.0F}, {8.0F, 8.0F}}, {0.0F, 0.0F}};
    const advanced_platformer::FlyingMovement movement{100.0F};
    advanced_platformer::InputIntentions intentions;
    intentions.direction.x = 1.0F;

    const advanced_platformer::CollisionContacts contacts =
        advanced_platformer::updateFlyingMovement(map, body, movement, intentions, 0.2F);

    REQUIRE(contacts.right);
    REQUIRE(body.bounds.topLeft.x == 24.0F);
    REQUIRE(body.velocity.x == 0.0F);
}

TEST_CASE("Flying movement rejects invalid timing and intentions", "[movement][flying]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"...", "...", "###"});
    advanced_platformer::Body body{{{16.0F, 16.0F}, {8.0F, 8.0F}}, {0.0F, 0.0F}};

    REQUIRE_THROWS_AS(
        advanced_platformer::updateFlyingMovement(map, body, {-1.0F}, {}, 0.1F),
        std::invalid_argument);

    advanced_platformer::InputIntentions invalid;
    invalid.direction.x = std::numeric_limits<float>::infinity();
    REQUIRE_THROWS_AS(
        advanced_platformer::updateFlyingMovement(map, body, {}, invalid, 0.1F),
        std::invalid_argument);
}
