#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_activity_scripts.hpp"
#include "advanced_platformer/npc/npc_system.hpp"
#include "advanced_platformer/world/level_room.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "advanced_platformer/world/world_requests.hpp"
#include "support/actor_builder.hpp"
#include "support/fixed_step.hpp"
#include "support/npc_machine_builder.hpp"
#include "support/recording_npc_scripts.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    constexpr glm::vec2 RoomOneFeet{40.0F, 64.0F};
    constexpr glm::vec2 RoomTwoFeet{104.0F, 64.0F};
    constexpr glm::vec2 RoomThreeFeet{168.0F, 64.0F};
    constexpr glm::vec2 DoorIntoRoomTwo{72.0F, 64.0F};
    constexpr glm::vec2 DoorIntoRoomThree{136.0F, 64.0F};

    advanced_platformer::TileMap threeRoomMap()
    {
        return tests::TileMapBuilder(
            {"#############", "#...#...#...#", "#...........#", "#...........#", "#############"});
    }

    std::vector<advanced_platformer::LevelRoom> threeRooms()
    {
        return {
            {.origin = {0, 0}, .size = {5, 5}},
            {.origin = {4, 0}, .size = {5, 5}},
            {.origin = {8, 0}, .size = {5, 5}}};
    }

    advanced_platformer::ActorId addExplorer(advanced_platformer::World& world, glm::vec2 feet)
    {
        return world.addActor(
            tests::ActorBuilder::sized({12.0F, 12.0F})
                .atFeet(feet)
                .platforming()
                .thinking({64.0F, 1.0F})
                .running(
                    tests::NpcMachineBuilder::named("explorer")
                        .state("exploring", advanced_platformer::NpcActivity{"fixture", "act"})));
    }

    advanced_platformer::NpcActivitySnapshot snapshotAt(
        const advanced_platformer::TileMap& map,
        advanced_platformer::World& world,
        advanced_platformer::ActorId explorer,
        glm::vec2 feet)
    {
        advanced_platformer::moveFeetTo(world.findActor(explorer)->body.bounds, feet);
        tests::RecordingNpcScripts scripts;
        advanced_platformer::WorldRequests requests;
        advanced_platformer::updateNpcBehaviour(
            map, world, requests, tests::FixedStepSeconds, scripts);
        return scripts.calls.back().snapshot;
    }

    std::vector<glm::vec2> pickupFeet(const advanced_platformer::NpcActivitySnapshot& snapshot)
    {
        std::vector<glm::vec2> feet;
        feet.reserve(snapshot.pickups.size());
        for (const advanced_platformer::NpcPickupSnapshot& pickup : snapshot.pickups)
        {
            feet.push_back(pickup.feet);
        }
        return feet;
    }
}

TEST_CASE("An NPC learns the exit only once it has entered the exit's room", "[npc][rooms]")
{
    const advanced_platformer::TileMap map = threeRoomMap();
    advanced_platformer::World world;
    world.setRooms(threeRooms());
    world.setExit({.bounds = {{160.0F, 48.0F}, {16.0F, 16.0F}}});
    const advanced_platformer::ActorId explorer = addExplorer(world, RoomOneFeet);

    REQUIRE_FALSE(snapshotAt(map, world, explorer, RoomOneFeet).exitFeet.has_value());
    REQUIRE_FALSE(snapshotAt(map, world, explorer, RoomTwoFeet).exitFeet.has_value());
    REQUIRE(snapshotAt(map, world, explorer, RoomThreeFeet).exitFeet == RoomThreeFeet);
    REQUIRE(snapshotAt(map, world, explorer, RoomOneFeet).exitFeet == RoomThreeFeet);
}

TEST_CASE("An NPC's snapshot lists only the pickups in rooms it has entered", "[npc][rooms]")
{
    const advanced_platformer::TileMap map = threeRoomMap();
    advanced_platformer::World world({{1, "Coin", {}, 5}});
    world.setRooms(threeRooms());
    world.addPickup({{{{36.0F, 56.0F}, {8.0F, 8.0F}}}, {1, 1}});
    world.addPickup({{{{100.0F, 56.0F}, {8.0F, 8.0F}}}, {1, 1}});
    world.addPickup({{{{164.0F, 56.0F}, {8.0F, 8.0F}}}, {1, 1}});
    const advanced_platformer::ActorId explorer = addExplorer(world, RoomOneFeet);

    REQUIRE(
        pickupFeet(snapshotAt(map, world, explorer, RoomOneFeet)) ==
        std::vector<glm::vec2>{RoomOneFeet});
    REQUIRE(
        pickupFeet(snapshotAt(map, world, explorer, RoomTwoFeet)) ==
        std::vector<glm::vec2>{RoomOneFeet, RoomTwoFeet});
}

TEST_CASE(
    "The unexplored doors lead out of entered rooms and shrink as the NPC enters the rooms beyond",
    "[npc][rooms]")
{
    const advanced_platformer::TileMap map = threeRoomMap();
    advanced_platformer::World world;
    world.setRooms(threeRooms());
    const advanced_platformer::ActorId explorer = addExplorer(world, RoomOneFeet);

    REQUIRE(
        snapshotAt(map, world, explorer, RoomOneFeet).unexploredDoors ==
        std::vector<glm::vec2>{DoorIntoRoomTwo});
    REQUIRE(
        snapshotAt(map, world, explorer, RoomTwoFeet).unexploredDoors ==
        std::vector<glm::vec2>{DoorIntoRoomThree});
    REQUIRE(snapshotAt(map, world, explorer, RoomThreeFeet).unexploredDoors.empty());
    REQUIRE(snapshotAt(map, world, explorer, RoomOneFeet).unexploredDoors.empty());
}

TEST_CASE("Each unexplored door counts the entered rooms between the NPC and it", "[npc][rooms]")
{
    const advanced_platformer::TileMap map = threeRoomMap();
    advanced_platformer::World world;
    world.setRooms(threeRooms());
    const advanced_platformer::ActorId explorer = addExplorer(world, RoomOneFeet);

    REQUIRE(
        snapshotAt(map, world, explorer, RoomOneFeet).roomsToUnexploredDoors ==
        std::vector<int>{0});
    REQUIRE(
        snapshotAt(map, world, explorer, RoomTwoFeet).roomsToUnexploredDoors ==
        std::vector<int>{0});
    REQUIRE(
        snapshotAt(map, world, explorer, RoomOneFeet).roomsToUnexploredDoors ==
        std::vector<int>{1});
}

TEST_CASE("A wall without an opening between two rooms is no door", "[npc][rooms]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"#########", "#...#...#", "#...#...#", "#...#...#", "#########"});
    advanced_platformer::World world;
    world.setRooms({{.origin = {0, 0}, .size = {5, 5}}, {.origin = {4, 0}, .size = {5, 5}}});
    const advanced_platformer::ActorId explorer = addExplorer(world, RoomOneFeet);

    REQUIRE(snapshotAt(map, world, explorer, RoomOneFeet).unexploredDoors.empty());
}

TEST_CASE("A level without rooms hides nothing from an NPC", "[npc][rooms]")
{
    const advanced_platformer::TileMap map = threeRoomMap();
    advanced_platformer::World world({{1, "Coin", {}, 5}});
    world.setExit({.bounds = {{160.0F, 48.0F}, {16.0F, 16.0F}}});
    world.addPickup({{{{164.0F, 56.0F}, {8.0F, 8.0F}}}, {1, 1}});
    const advanced_platformer::ActorId explorer = addExplorer(world, RoomOneFeet);

    const advanced_platformer::NpcActivitySnapshot snapshot =
        snapshotAt(map, world, explorer, RoomOneFeet);
    REQUIRE(snapshot.exitFeet == RoomThreeFeet);
    REQUIRE(pickupFeet(snapshot) == std::vector<glm::vec2>{RoomThreeFeet});
    REQUIRE(snapshot.unexploredDoors.empty());
}
