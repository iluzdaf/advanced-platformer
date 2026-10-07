#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/inventory/inventory.hpp"
#include "advanced_platformer/inventory/item.hpp"
#include "advanced_platformer/physics/body.hpp"
#include "advanced_platformer/world/pickup.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "advanced_platformer/world/world_requests.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/require_near.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    std::vector<advanced_platformer::ItemDefinition> items()
    {
        return {
            {1, "Coin", {}, 5},
            {2, "Potion", {}, 5, advanced_platformer::ItemEffect::Heal, 2},
            {3, "Key", {}, 1}};
    }

    advanced_platformer::Pickup pickupAt(
        glm::vec2 position,
        glm::vec2 size,
        advanced_platformer::ItemStack stack)
    {
        advanced_platformer::Pickup pickup;
        pickup.body.bounds = {position, size};
        pickup.stack = stack;
        return pickup;
    }

    advanced_platformer::World makeWorld()
    {
        advanced_platformer::World world(items());
        tests::addPlayer(
            world,
            tests::ActorBuilder::sized({12.0F, 16.0F})
                .atFeet({22.0F, 32.0F})
                .platforming()
                .withHealth(1, 3)
                .withInventory(advanced_platformer::Inventory(2)));
        return world;
    }

    void collect(advanced_platformer::World& world)
    {
        advanced_platformer::WorldRequests requests;
        advanced_platformer::updatePickups(world, requests);
        advanced_platformer::applyWorldRequests(world, requests);
        REQUIRE(requests.empty());
    }
}

TEST_CASE(
    "Automatic pickups collect overlapping items only after requests are applied",
    "[world][pickups]")
{
    auto world = makeWorld();
    world.addPickup(pickupAt({18.0F, 20.0F}, {8.0F, 8.0F}, {1, 3}));
    world.addPickup(pickupAt({80.0F, 20.0F}, {8.0F, 8.0F}, {1, 2}));
    advanced_platformer::WorldRequests requests;
    advanced_platformer::updatePickups(world, requests);
    REQUIRE(tests::component<advanced_platformer::Inventory>(tests::player(world)).count(1) == 0);
    REQUIRE(world.pickups().size() == 2);
    advanced_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::component<advanced_platformer::Inventory>(tests::player(world)).count(1) == 3);
    REQUIRE(world.pickups().size() == 1);
    REQUIRE(world.pickups().front().body.bounds.topLeft.x == 80.0F);
    REQUIRE(requests.empty());
}

TEST_CASE(
    "Partial pickups stay in the world and can be collected after freeing space",
    "[world][pickups]")
{
    auto world = makeWorld();
    tests::player(world).inventory = advanced_platformer::Inventory(1);
    tests::component<advanced_platformer::Inventory>(tests::player(world))
        .add(world.itemDefinition(1), 4);
    world.addPickup(pickupAt({18.0F, 20.0F}, {8.0F, 8.0F}, {1, 4}));
    collect(world);
    REQUIRE(tests::component<advanced_platformer::Inventory>(tests::player(world)).count(1) == 5);
    REQUIRE(world.pickups().front().stack.quantity == 3);
    collect(world);
    REQUIRE(world.pickups().front().stack.quantity == 3);
    tests::component<advanced_platformer::Inventory>(tests::player(world)).remove(1, 3);
    collect(world);
    REQUIRE(world.pickups().empty());
    REQUIRE(tests::component<advanced_platformer::Inventory>(tests::player(world)).count(1) == 5);
}

TEST_CASE(
    "Multiple overlapping pickups and duplicate requests do not skip or duplicate items",
    "[world][pickups]")
{
    auto world = makeWorld();
    world.addPickup(pickupAt({18.0F, 20.0F}, {8.0F, 8.0F}, {1, 2}));
    world.addPickup(pickupAt({18.0F, 20.0F}, {8.0F, 8.0F}, {2, 1}));
    advanced_platformer::WorldRequests requests;
    advanced_platformer::updatePickups(world, requests);
    advanced_platformer::updatePickups(world, requests);
    advanced_platformer::applyWorldRequests(world, requests);
    REQUIRE(world.pickups().empty());
    REQUIRE(tests::component<advanced_platformer::Inventory>(tests::player(world)).count(1) == 2);
    REQUIRE(tests::component<advanced_platformer::Inventory>(tests::player(world)).count(2) == 1);
}

TEST_CASE("Dead players and players without inventory do not collect pickups", "[world][pickups]")
{
    auto world = makeWorld();
    world.addPickup(pickupAt({18.0F, 20.0F}, {8.0F, 8.0F}, {1, 2}));
    tests::player(world).life = advanced_platformer::LifeState::Dying;
    collect(world);
    REQUIRE(world.pickups().size() == 1);
    tests::player(world).life = advanced_platformer::LifeState::Alive;
    tests::player(world).inventory.reset();
    collect(world);
    REQUIRE(world.pickups().size() == 1);
}

TEST_CASE("World rejects invalid pickup data", "[world][pickups][validation]")
{
    auto world = makeWorld();
    REQUIRE_THROWS_AS(
        world.addPickup(pickupAt({0.0F, 0.0F}, {0.0F, 1.0F}, {1, 1})), std::invalid_argument);
    REQUIRE_THROWS_AS(
        world.addPickup(pickupAt({0.0F, 0.0F}, {1.0F, 1.0F}, {99, 1})), std::invalid_argument);
    REQUIRE_THROWS_AS(
        world.addPickup(pickupAt({0.0F, 0.0F}, {1.0F, 1.0F}, {1, 0})), std::invalid_argument);
}

TEST_CASE("A pickup falls until it rests on a tile", "[world][pickups]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"....", "....", "####"});
    advanced_platformer::World world(items());
    world.addPickup(pickupAt({4.0F, 4.0F}, {8.0F, 8.0F}, {1, 1}));
    const advanced_platformer::Pickup& pickup = world.pickups().front();

    advanced_platformer::updatePickupMovement(map, world, 0.1F);

    REQUIRE_NEAR(pickup.body.velocity.y, advanced_platformer::DefaultGravity * 0.1F);
    REQUIRE_NEAR(pickup.body.bounds.topLeft.y, 4.0F + pickup.body.velocity.y * 0.1F);

    for (int step = 0; step < 10; ++step)
    {
        advanced_platformer::updatePickupMovement(map, world, 0.1F);
    }

    // Resting on the floor, whose top edge is two tiles down.
    REQUIRE_NEAR(pickup.body.bounds.topLeft.y, 24.0F);
    REQUIRE_NEAR(pickup.body.velocity.y, 0.0F);
    REQUIRE_NEAR(pickup.body.bounds.topLeft.x, 4.0F);
}

TEST_CASE("A pickup falls through the tile that breaks beneath it", "[world][pickups]")
{
    advanced_platformer::TileMap map =
        tests::TileMapBuilder({"....", "XXXX", "....", "####"})
            .where('X', tests::Tile().blocksMovement().breaksInto('.'));
    advanced_platformer::World world(items());
    world.addPickup(pickupAt({4.0F, 8.0F}, {8.0F, 8.0F}, {1, 1}));
    const advanced_platformer::Pickup& pickup = world.pickups().front();

    advanced_platformer::updatePickupMovement(map, world, 0.1F);
    REQUIRE_NEAR(pickup.body.bounds.topLeft.y, 8.0F);

    REQUIRE(map.breakTile({0, 1}));
    for (int step = 0; step < 12; ++step)
    {
        advanced_platformer::updatePickupMovement(map, world, 0.1F);
    }

    REQUIRE_NEAR(pickup.body.bounds.topLeft.y, 40.0F);
    REQUIRE_NEAR(pickup.body.velocity.y, 0.0F);
}

TEST_CASE("Pickup movement rejects a negative step", "[world][pickups]")
{
    const advanced_platformer::TileMap map = tests::TileMapBuilder({"..", "##"});
    advanced_platformer::World world(items());
    REQUIRE_THROWS_AS(
        advanced_platformer::updatePickupMovement(map, world, -0.1F), std::invalid_argument);
}
