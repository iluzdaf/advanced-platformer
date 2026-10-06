#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <stdexcept>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/world/level_exit.hpp"
#include "advanced_platformer/world/level_validation.hpp"
#include "advanced_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/add_player.hpp"
#include "support/fixed_step.hpp"
#include "support/tile_size.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    tests::ActorBuilder makePlatformer(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 20.0F}).atFeet(feet).platforming();
    }

    // A world whose player stands in one cell and whose exit stands in another.
    advanced_platformer::World worldWithExit(
        advanced_platformer::Cell player,
        advanced_platformer::Cell exit)
    {
        advanced_platformer::World world;
        tests::addPlayer(
            world, makePlatformer(advanced_platformer::feetInCell(tests::TileSize, player)));
        advanced_platformer::LevelExit levelExit;
        levelExit.bounds = advanced_platformer::boxStandingOn(
            advanced_platformer::feetInCell(tests::TileSize, exit), {16.0F, 16.0F});
        world.setExit(levelExit);
        return world;
    }

    tests::ActorBuilder makeFlyer(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 8.0F}).atFeet(feet).flying(0.0F);
    }
}

TEST_CASE("Level actors require clear spawn positions", "[world][level-validation]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"###", "...", "###"});
    advanced_platformer::World world;
    world.addActor(makePlatformer({24.0F, 32.0F}));

    REQUIRE_THROWS_AS(
        advanced_platformer::validateLevelActors(map, world, 1), std::invalid_argument);
}

TEST_CASE("Platformer spawns and patrol points require ground support", "[world][level-validation]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});

    SECTION("spawn")
    {
        advanced_platformer::World world;
        world.addActor(makePlatformer({24.0F, 16.0F}));
        REQUIRE_THROWS_AS(
            advanced_platformer::validateLevelActors(map, world, 1), std::invalid_argument);
    }

    SECTION("patrol point")
    {
        advanced_platformer::World world;
        world.addActor(
            makePlatformer({24.0F, 32.0F}).thinking({}).patrolling({16.0F, 32.0F}, {16.0F, 16.0F}));
        REQUIRE_THROWS_AS(
            advanced_platformer::validateLevelActors(map, world, 1), std::invalid_argument);
    }
}

TEST_CASE("The player respawn requires clearance and ground support", "[world][level-validation]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});

    SECTION("blocked respawn")
    {
        advanced_platformer::World world;
        const auto player = world.addActor(makePlatformer({24.0F, 32.0F}));
        world.setPlayer(player, {24.0F, 48.0F});

        REQUIRE_THROWS_WITH(
            advanced_platformer::validateLevelActors(map, world, 7),
            "Level 7 actor 1 respawn overlaps a blocked tile");
    }

    SECTION("unsupported respawn")
    {
        advanced_platformer::World world;
        const auto player = world.addActor(makePlatformer({24.0F, 32.0F}));
        world.setPlayer(player, {24.0F, 16.0F});

        REQUIRE_THROWS_WITH(
            advanced_platformer::validateLevelActors(map, world, 7),
            "Level 7 actor 1 respawn has no ground support");
    }
}

TEST_CASE("Flying actors require clearance but not ground support", "[world][level-validation]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"...", "...", "###"});
    advanced_platformer::World world;
    world.addActor(
        makeFlyer({24.0F, 16.0F}).thinking({}).patrolling({24.0F, 16.0F}, {32.0F, 24.0F}));

    REQUIRE_NOTHROW(advanced_platformer::validateLevelActors(map, world, 1));
}

TEST_CASE("A climber's patrol points need clearance but not ground", "[world][level-validation]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"#####", ".....", ".....", ".....", "#####"});
    advanced_platformer::Actor climber = makePlatformer({24.0F, 64.0F})
                                             .climbing()
                                             .thinking({})
                                             .patrolling({24.0F, 64.0F}, {40.0F, 48.0F});

    SECTION("patrol point in the air")
    {
        advanced_platformer::World world;
        world.addActor(climber);
        REQUIRE_NOTHROW(advanced_platformer::validateLevelActors(map, world, 1));
    }

    SECTION("spawn in the air")
    {
        climber.body.bounds.topLeft.y -= 16.0F;
        advanced_platformer::World world;
        world.addActor(climber);
        REQUIRE_THROWS_WITH(
            advanced_platformer::validateLevelActors(map, world, 1),
            "Level 1 actor 1 spawn has no ground support");
    }
}

TEST_CASE("The player can reach an exit it can walk and jump to", "[world][level-validation]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder(
        {"##########", "#........#", "#........#", "#.....##.#", "##########"});
    const advanced_platformer::World world = worldWithExit({1, 3}, {6, 2});

    REQUIRE(advanced_platformer::playerCanReachExit(map, world, tests::FixedStepSeconds));
    REQUIRE(world.platformerConnections().size() == 0);
}

TEST_CASE("The player cannot reach an exit walled off from it", "[world][level-validation]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder(
        {"##########", "#....#...#", "#....#...#", "#....#...#", "##########"});

    REQUIRE_FALSE(
        advanced_platformer::playerCanReachExit(
            map, worldWithExit({1, 3}, {7, 3}), tests::FixedStepSeconds));
}

TEST_CASE("The player cannot reach an exit high above its jump", "[world][level-validation]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder(
        {"##########",
         "#........#",
         "#........#",
         "#......###",
         "#........#",
         "#........#",
         "#........#",
         "#........#",
         "##########"});

    REQUIRE_FALSE(
        advanced_platformer::playerCanReachExit(
            map, worldWithExit({1, 7}, {7, 2}), tests::FixedStepSeconds));
}
