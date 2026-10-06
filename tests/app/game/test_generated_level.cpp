#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>

#include "game/game.hpp"
#include "lua_presentation_script.hpp"
#include "content/game_catalogs.hpp"
#include "content/level_catalog.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "support/atlas_size.hpp"
#include "support/fixed_step.hpp"

namespace
{
    advanced_platformer::Game gameFrom(const char* levelCatalog)
    {
        return {
            0,
            advanced_platformer::loadLevelCatalog(levelCatalog),
            advanced_platformer::loadGameCatalogs("tests/fixtures/catalogs", tests::AtlasSize),
            advanced_platformer::LuaNpcScripts{},
            advanced_platformer::LuaPresentationScript{},
            tests::FixedStepSeconds};
    }
}

TEST_CASE("A generated level starts from its catalog seed", "[app][generation]")
{
    const advanced_platformer::Game game = gameFrom("tests/fixtures/levels/generated_levels.json");

    REQUIRE(game.levelNumber() == 1);
    REQUIRE(game.levelSeed() == 7U);
    REQUIRE(game.levelJson().contains("\"nextLevel\": 2"));
}

TEST_CASE(
    "Rerolling builds the level from the next seed and restarting keeps it",
    "[app][generation]")
{
    advanced_platformer::Game game = gameFrom("tests/fixtures/levels/generated_levels.json");
    const std::string first = game.levelJson();
    const advanced_platformer::Health health = game.playerHealth();

    REQUIRE(game.rerollLevel());
    REQUIRE(game.levelNumber() == 1);
    REQUIRE(game.levelSeed() == 8U);
    REQUIRE(game.playerHealth().current == health.current);
    const std::string second = game.levelJson();
    REQUIRE(second != first);

    game.restartLevel();
    REQUIRE(game.levelSeed() == 8U);
    REQUIRE(game.levelJson() == second);
}

TEST_CASE("A level read from a file has no seed and cannot be rerolled", "[app][generation]")
{
    advanced_platformer::Game game = gameFrom("tests/fixtures/levels/levels.json");
    const std::string level = game.levelJson();

    REQUIRE_FALSE(game.levelSeed().has_value());
    REQUIRE_FALSE(game.rerollLevel());
    REQUIRE(game.levelJson() == level);
}
