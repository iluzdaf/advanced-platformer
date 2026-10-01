#include <catch2/catch_message.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <string_view>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/navigation/path_follower.hpp"
#include "advanced_platformer/timing/frame_profile.hpp"
#include "advanced_platformer/inventory/item.hpp"
#include "advanced_platformer/world/pickup.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "advanced_platformer/world/world_simulation.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/pursuer_npc.hpp"
#include "lua_npc_scripts.hpp"
#include "support/recording_npc_scripts.hpp"
#include "support/tile_map_builder.hpp"
#include "support/add_player.hpp"
#include "support/fixed_step.hpp"

TEST_CASE("World simulation advances its shared clock once per update", "[world][simulation][time]")
{
    advanced_platformer::TileMap map = tests::TileMapBuilder({"."});
    advanced_platformer::World world;
    tests::RecordingNpcScripts scripts;

    advanced_platformer::updateWorldSimulation(map, world, 0.25F, scripts);
    advanced_platformer::updateWorldSimulation(map, world, 0.25F, scripts);

    REQUIRE(world.simulationTimeSeconds() == 0.5F);

    world.completeLevel();
    advanced_platformer::updateWorldSimulation(map, world, 0.25F, scripts);

    REQUIRE(world.simulationTimeSeconds() == 0.5F);
}

TEST_CASE("World simulation spawns a projectile after projectile movement", "[world][simulation]")
{
    advanced_platformer::TileMap map = tests::TileMapBuilder({".....", ".....", "#####"});
    advanced_platformer::World world;
    advanced_platformer::Actor player = tests::ActorBuilder::sized({12.0F, 12.0F})
                                            .atFeet({22.0F, 28.0F})
                                            .platforming()
                                            .onTeam(advanced_platformer::Team::Player)
                                            .shooting();
    player.intentions.aimDirection = {1.0F, 0.0F};
    player.intentions.primaryAttackPressed = true;
    const advanced_platformer::ActorId playerId = world.addActor(player);
    tests::RecordingNpcScripts scripts;

    advanced_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds, scripts);

    REQUIRE(world.projectiles().size() == 1);
    const float spawnPosition = world.projectiles().front().bounds.topLeft.x;

    advanced_platformer::Actor& storedPlayer = tests::actor(world, playerId);
    storedPlayer.intentions.primaryAttackPressed = false;
    advanced_platformer::updateWorldSimulation(map, world, tests::FixedStepSeconds, scripts);

    REQUIRE(world.projectiles().size() == 1);
    REQUIRE(world.projectiles().front().bounds.topLeft.x > spawnPosition);
}

TEST_CASE("World simulation lets a pickup fall onto the tile below", "[world][simulation]")
{
    advanced_platformer::TileMap map = tests::TileMapBuilder({"....", "....", "####"});
    advanced_platformer::World world({{1, "Coin", {}, 5}});
    advanced_platformer::Pickup pickup;
    pickup.body.bounds = {{4.0F, 4.0F}, {8.0F, 8.0F}};
    pickup.stack = {1, 1};
    world.addPickup(pickup);
    tests::RecordingNpcScripts scripts;

    for (int step = 0; step < 12; ++step)
    {
        advanced_platformer::updateWorldSimulation(map, world, 0.1F, scripts);
    }

    REQUIRE(world.pickups().front().body.bounds.topLeft.y == 24.0F);
}

TEST_CASE(
    "Profiling a step changes nothing about what it simulates",
    "[world][simulation][profile]")
{
    const auto makeWorld = []
    {
        advanced_platformer::World world;
        tests::addPlayer(
            world,
            tests::ActorBuilder::sized({12.0F, 12.0F})
                .inCell({1, 1})
                .platforming()
                .onTeam(advanced_platformer::Team::Player));
        world.addActor(
            tests::ActorBuilder::sized({12.0F, 12.0F})
                .inCell({6, 1})
                .flying(60.0F)
                .onTeam(advanced_platformer::Team::Enemy)
                .thinking({96.0F, 1.0F})
                .running(tests::pursuerMachine()));
        return world;
    };
    advanced_platformer::LuaNpcScripts scripts;
    tests::loadPursuerScript(scripts);
    advanced_platformer::TileMap timedMap =
        tests::TileMapBuilder({"........", "........", "########"});
    advanced_platformer::TileMap plainMap = timedMap;
    advanced_platformer::World timed = makeWorld();
    advanced_platformer::World plain = makeWorld();
    advanced_platformer::FrameProfile profile;

    for (int tick = 0; tick < 30; ++tick)
    {
        advanced_platformer::updateWorldSimulation(
            timedMap, timed, tests::FixedStepSeconds, scripts, &profile);
        advanced_platformer::updateWorldSimulation(
            plainMap, plain, tests::FixedStepSeconds, scripts);
    }

    REQUIRE(timed.simulationTimeSeconds() == plain.simulationTimeSeconds());
    REQUIRE(timed.actors().size() == plain.actors().size());
    for (std::size_t index = 0; index < timed.actors().size(); ++index)
    {
        REQUIRE(
            timed.actors()[index].body.bounds.topLeft == plain.actors()[index].body.bounds.topLeft);
    }
    // The chasing NPC searched for a path at least once.
    REQUIRE(advanced_platformer::frameStatisticCount(profile, "Path searches") >= 1);
    REQUIRE(
        std::ranges::any_of(
            profile.phases,
            [](const advanced_platformer::PhaseTiming& phase)
            { return std::string_view(phase.name) == "Path search"; }));
    REQUIRE(
        std::ranges::all_of(
            profile.phases,
            [](const advanced_platformer::PhaseTiming& phase) { return phase.seconds >= 0.0F; }));
    REQUIRE(profile.nestedSecondsOfOpenPhases.empty());
}
