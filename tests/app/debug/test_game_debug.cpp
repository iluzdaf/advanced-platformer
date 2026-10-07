#include <catch2/catch_test_macros.hpp>

#include "game/game.hpp"
#include "support/fixture_game.hpp"

TEST_CASE("Breaking a tile under a position breaks nothing that cannot break", "[app][debug]")
{
    advanced_platformer::Game game = tests::fixtureGame("tests/fixtures/rooms/rooms/rooms.json");
    REQUIRE_FALSE(game.breakTileAt({-100.0F, -100.0F}));
    REQUIRE_FALSE(game.breakTileAt({8.0F, 8.0F}));
}
