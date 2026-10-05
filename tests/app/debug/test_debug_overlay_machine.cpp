#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "debug/debug_overlay.hpp"
#include "debug/navigation_debug.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/render/camera.hpp"
#include "advanced_platformer/world/world.hpp"
#include "advanced_platformer/world/pickup.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "support/actor_builder.hpp"
#include "support/npc_facts_builder.hpp"
#include "support/npc_machine_builder.hpp"
#include "support/actor_components.hpp"
#include "support/tile_map_builder.hpp"
#include "support/add_player.hpp"
#include "support/fixed_step.hpp"

TEST_CASE("The overlay shows an NPC's machine state", "[app][debug]")
{
    advanced_platformer::World world;
    world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .at({16.0F, 32.0F})
            .platforming()
            .thinking({})
            .running(
                tests::NpcMachineBuilder::named("test").state(
                    "rest", tests::testActivity("idle"))));
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"......", "######"});
    const advanced_platformer::CameraController cameraController{
        advanced_platformer::Camera{}, {80.0F, 40.0F}};

    const advanced_platformer::DebugOverlay debug = advanced_platformer::makeDebugOverlay(
        world, map, cameraController, 128.0F, tests::FixedStepSeconds);

    REQUIRE(debug.actors.size() == 1);
    REQUIRE(debug.actors.front().machineState == "rest");
}

namespace
{
    // An NPC on the ground running a two-state machine, for the machine window's tests.
    advanced_platformer::Actor machineNpc(glm::vec2 topLeft)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .at(topLeft)
            .platforming()
            .thinking({})
            .running(
                tests::NpcMachineBuilder::named("test")
                    .state("rest", tests::testActivity("idle"))
                    .state("hunt", tests::testActivity("chase"))
                    .transition("rest", "hunt")
                    .when("targetKnown", true));
    }

    // Which NPC the machine window follows, or nothing.
    std::optional<advanced_platformer::ActorId> followedBy(
        const advanced_platformer::DebugOverlay& debug)
    {
        if (!debug.machine.has_value())
        {
            return std::nullopt;
        }
        return debug.machine.value_or(advanced_platformer::MachineDebugInfo{}).actor;
    }

    advanced_platformer::DebugOverlay overlayOf(
        const advanced_platformer::World& world,
        std::optional<glm::vec2> cursorWorld = std::nullopt,
        std::optional<advanced_platformer::ActorId> lockedMachineActor = std::nullopt)
    {
        const advanced_platformer::TileMap map = tests::TileMapBuilder({"......", "######"});
        const advanced_platformer::CameraController cameraController{
            advanced_platformer::Camera{}, {80.0F, 40.0F}};
        advanced_platformer::NavigationDebugView view;
        view.cursorWorld = cursorWorld;
        return advanced_platformer::makeDebugOverlay(
            world,
            map,
            cameraController,
            128.0F,
            tests::FixedStepSeconds,
            view,
            lockedMachineActor);
    }
}

TEST_CASE("The machine window follows the NPC nearest the player", "[app][debug]")
{
    advanced_platformer::World world;
    tests::addPlayer(
        world, tests::ActorBuilder::sized({12.0F, 12.0F}).at({100.0F, 20.0F}).platforming());
    world.addActor(machineNpc({200.0F, 20.0F}));
    const advanced_platformer::ActorId nearer = world.addActor(machineNpc({60.0F, 20.0F}));

    const advanced_platformer::DebugOverlay debug = overlayOf(world);

    REQUIRE(debug.machine.has_value());
    const advanced_platformer::MachineDebugInfo machine =
        debug.machine.value_or(advanced_platformer::MachineDebugInfo{});
    REQUIRE(machine.actor == nearer);
    REQUIRE(machine.definition.name == "test");
    REQUIRE(machine.definition.states.size() == 2);
    REQUIRE(machine.definition.transitions.size() == 1);
    REQUIRE(machine.active == 0);
    REQUIRE_FALSE(machine.lastFired.has_value());
}

TEST_CASE("The machine window follows the NPC under the cursor instead", "[app][debug]")
{
    advanced_platformer::World world;
    tests::addPlayer(
        world, tests::ActorBuilder::sized({12.0F, 12.0F}).at({100.0F, 20.0F}).platforming());
    world.addActor(machineNpc({60.0F, 20.0F}));
    const advanced_platformer::ActorId further = world.addActor(machineNpc({200.0F, 20.0F}));

    const advanced_platformer::DebugOverlay underCursor =
        overlayOf(world, glm::vec2{206.0F, 26.0F});
    REQUIRE(followedBy(underCursor) == further);
    // The cursor over nothing changes nothing.
    REQUIRE(followedBy(overlayOf(world, glm::vec2{10.0F, 10.0F})) != further);
}

TEST_CASE("A locked machine actor overrides the cursor", "[app][debug]")
{
    advanced_platformer::World world;
    tests::addPlayer(
        world, tests::ActorBuilder::sized({12.0F, 12.0F}).at({100.0F, 20.0F}).platforming());
    const advanced_platformer::ActorId locked = world.addActor(machineNpc({60.0F, 20.0F}));
    world.addActor(machineNpc({200.0F, 20.0F}));

    const advanced_platformer::DebugOverlay debug =
        overlayOf(world, glm::vec2{206.0F, 26.0F}, locked);

    REQUIRE(followedBy(debug) == locked);
}

TEST_CASE("The machine window follows nothing off screen", "[app][debug]")
{
    advanced_platformer::World world;
    tests::addPlayer(
        world, tests::ActorBuilder::sized({12.0F, 12.0F}).at({100.0F, 20.0F}).platforming());
    const advanced_platformer::ActorId offScreen =
        world.addActor(machineNpc({advanced_platformer::InternalViewportSize.x + 100.0F, 20.0F}));

    REQUIRE_FALSE(overlayOf(world).machine.has_value());
    REQUIRE(followedBy(overlayOf(world, std::nullopt, offScreen)) == offScreen);
}

TEST_CASE("The machine window is told which transition fired last", "[app][debug]")
{
    advanced_platformer::World world;
    const advanced_platformer::ActorId id = world.addActor(machineNpc({60.0F, 20.0F}));
    REQUIRE(
        advanced_platformer::advanceNpcMachine(
            tests::component<advanced_platformer::NpcMachine>(world, id),
            tests::NpcFactsBuilder::facts().knowingTarget(),
            tests::FixedStepSeconds) == 0);

    const advanced_platformer::DebugOverlay debug = overlayOf(world);

    REQUIRE(debug.machine.has_value());
    const advanced_platformer::MachineDebugInfo machine =
        debug.machine.value_or(advanced_platformer::MachineDebugInfo{});
    REQUIRE(machine.active == 1);
    REQUIRE(machine.lastFired == 0);
}
