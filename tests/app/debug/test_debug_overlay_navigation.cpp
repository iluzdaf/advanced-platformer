#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "debug/debug_overlay.hpp"
#include "debug/navigation_debug.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/navigation/route.hpp"
#include "advanced_platformer/navigation/platformer_connections.hpp"
#include "advanced_platformer/navigation/platformer_traversal_profile.hpp"
#include "advanced_platformer/navigation/traversal.hpp"
#include "advanced_platformer/render/camera.hpp"
#include "advanced_platformer/world/world.hpp"
#include "advanced_platformer/world/pickup.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "support/cell_connections.hpp"
#include "support/actor_builder.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"
#include "support/fixed_step.hpp"
#include "support/navigation_paths.hpp"
#include "support/route_connections.hpp"

TEST_CASE("Debug overlay data describes path connections and progress", "[app][debug]")
{
    advanced_platformer::PathFollower follower;
    follower.path = tests::floorPath(
        {1, 2},
        {{{{3, 2}}, advanced_platformer::Traversal::Walk, {}},
         {{{4, 1}}, advanced_platformer::Traversal::Jump, {}},
         {{{4, 3}}, advanced_platformer::Traversal::Fall, {}}});
    follower.nextStep = 1;
    follower.goal = advanced_platformer::feetInCell(tests::TileSize, {4, 3});

    advanced_platformer::Actor npc =
        tests::ActorBuilder::sized({12.0F, 12.0F}).at({16.0F, 32.0F}).platforming().thinking({});
    npc.pathFollower = follower;

    advanced_platformer::World world;
    world.addActor(npc);
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"......", "######"});
    const advanced_platformer::CameraController cameraController{
        advanced_platformer::Camera{}, {80.0F, 40.0F}};

    const advanced_platformer::DebugOverlay debug = advanced_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

    REQUIRE(debug.actors.size() == 1);
    REQUIRE(debug.actors.front().pathFollower.has_value());
    const advanced_platformer::PathFollowerDebugInfo path =
        debug.actors.front().pathFollower.value_or(advanced_platformer::PathFollowerDebugInfo{});
    REQUIRE(path.hasPath);
    REQUIRE(path.nextStep == 1);
    REQUIRE(path.stepCount == 3);
    REQUIRE(path.goalFeet == advanced_platformer::feetInCell(tests::TileSize, {4, 3}));
    REQUIRE(path.connections.size() == 3);

    REQUIRE(
        path.connections[0].fromFeet == advanced_platformer::feetInCell(tests::TileSize, {1, 2}));
    REQUIRE(path.connections[0].toFeet == advanced_platformer::feetInCell(tests::TileSize, {3, 2}));
    REQUIRE(path.connections[0].traversal == advanced_platformer::Traversal::Walk);
    REQUIRE(path.connections[0].completed);
    REQUIRE_FALSE(path.connections[0].next);

    REQUIRE(
        path.connections[1].fromFeet == advanced_platformer::feetInCell(tests::TileSize, {3, 2}));
    REQUIRE(path.connections[1].toFeet == advanced_platformer::feetInCell(tests::TileSize, {4, 1}));
    REQUIRE(path.connections[1].traversal == advanced_platformer::Traversal::Jump);
    REQUIRE_FALSE(path.connections[1].completed);
    REQUIRE(path.connections[1].next);

    REQUIRE(
        path.connections[2].fromFeet == advanced_platformer::feetInCell(tests::TileSize, {4, 1}));
    REQUIRE(path.connections[2].toFeet == advanced_platformer::feetInCell(tests::TileSize, {4, 3}));
    REQUIRE(path.connections[2].traversal == advanced_platformer::Traversal::Fall);
    REQUIRE_FALSE(path.connections[2].completed);
    REQUIRE_FALSE(path.connections[2].next);
}

TEST_CASE("Debug overlay data samples the simulated jump curve", "[app][debug]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const advanced_platformer::PlatformerMovementConfig movementConfig;
    const std::vector<advanced_platformer::RouteConnection> connections =
        tests::connectionsFrom(
            map,
            {2, 2},
            advanced_platformer::PlatformerTraversalProfile{
                .size = {12.0F, 12.0F},
                .movement = movementConfig,
                .stepSeconds = tests::FixedStepSeconds})
            .connections;
    const advanced_platformer::RouteConnection& jump =
        tests::connectionWith(connections, advanced_platformer::Traversal::Jump);

    advanced_platformer::Actor npc = tests::ActorBuilder::sized({12.0F, 12.0F})
                                         .at({0.0F, 0.0F})
                                         .platforming(movementConfig)
                                         .thinking({});
    npc.pathFollower = advanced_platformer::PathFollower{
        .path = tests::floorPath({2, 2}, {jump.step}),
        .goal = advanced_platformer::feetInCell(tests::TileSize, jump.step.destination.cell)};

    advanced_platformer::World world;
    world.addActor(npc);
    const advanced_platformer::CameraController cameraController{
        advanced_platformer::Camera{}, {80.0F, 40.0F}};

    const advanced_platformer::DebugOverlay debug = advanced_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);
    const advanced_platformer::PathFollowerDebugInfo path =
        debug.actors.front().pathFollower.value_or(advanced_platformer::PathFollowerDebugInfo{});

    REQUIRE(path.connections.size() == 1);
    REQUIRE(path.connections.front().sampledFeet.size() > 2);
    const float takeoffY = advanced_platformer::feetInCell(tests::TileSize, {2, 2}).y;
    const bool risesAboveTakeoff = std::ranges::any_of(
        path.connections.front().sampledFeet,
        [takeoffY](glm::vec2 feet) { return feet.y < takeoffY; });
    REQUIRE(risesAboveTakeoff);
}

TEST_CASE("The overlay shows only navigation cells near the camera", "[app][debug]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........................", "########################"});
    advanced_platformer::World world;
    world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet({8.0F, 16.0F})
            .platforming()
            .thinking({64.0F, 1.0F}));
    const advanced_platformer::CameraController cameraController{
        advanced_platformer::Camera{}, {80.0F, 40.0F}};

    const advanced_platformer::DebugOverlay debug = advanced_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

    REQUIRE(debug.navigationCache.has_value());
    const advanced_platformer::NavigationCacheDebugInfo navigation =
        debug.navigationCache.value_or(advanced_platformer::NavigationCacheDebugInfo{});
    const std::vector<advanced_platformer::NavigationCellDebugInfo>& cells = navigation.cells;
    REQUIRE(cells.size() == 21);
    REQUIRE(cells.front().bounds.topLeft == glm::vec2{0.0F, 0.0F});
    REQUIRE(cells.back().bounds.topLeft == glm::vec2{320.0F, 0.0F});
}
