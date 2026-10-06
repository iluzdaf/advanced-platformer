#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>
#include <format>
#include <stdexcept>
#include <utility>

#include "content/game_catalogs.hpp"
#include "content/game_content.hpp"
#include "content/room_pieces.hpp"
#include "game/game.hpp"
#include "game/level_reload.hpp"
#include "support/fixed_step.hpp"
#include "support/fixture_game.hpp"

TEST_CASE("A reload applies new definitions to the running game", "[app][reload]")
{
    advanced_platformer::Game running = tests::fixtureGame();
    advanced_platformer::GameContent changed = tests::fixtureContent();
    changed.gameCatalogs.actors.definitions.at("test_player").health = 5;

    const advanced_platformer::LevelReload reload = running.reload(std::move(changed));

    REQUIRE(running.playerHealth().maximum == 5);
    REQUIRE(running.playerHealth().current == 3);
    REQUIRE(reload.kept == 1);
    REQUIRE(reload.spawned.empty());
    REQUIRE(reload.removed.empty());
}

TEST_CASE("A failed reload leaves the game as it was", "[app][reload]")
{
    advanced_platformer::Game running = tests::fixtureGame();
    advanced_platformer::GameContent broken = tests::fixtureContent();
    broken.gameCatalogs.actors.definitions.erase("test_guard");
    broken.gameCatalogs.actors.definitions.at("test_player").health = 5;

    REQUIRE_THROWS_AS(running.reload(std::move(broken)), std::invalid_argument);

    REQUIRE(running.playerHealth().maximum == 3);
    running.update({}, tests::FixedStepSeconds);
    REQUIRE(running.reload(tests::fixtureContent()).kept == 1);
}

TEST_CASE("A reload that shuts off the exit leaves the game as it was", "[app][reload]")
{
    advanced_platformer::Game running =
        tests::fixtureGame("tests/fixtures/levels/rooms_some_shut.json");
    const std::uint32_t seed = running.levelSeed();
    advanced_platformer::GameContent shut = tests::fixtureContent();
    shut.pieces =
        advanced_platformer::loadRoomPieceCatalog("tests/fixtures/levels/rooms_all_shut.json");

    REQUIRE_THROWS_WITH(
        running.reload(std::move(shut)),
        std::format("Level 1: seed {} has no route from the spawn to the exit", seed));

    REQUIRE(running.levelSeed() == seed);
    REQUIRE(running.levelNumber() == 1);
}
