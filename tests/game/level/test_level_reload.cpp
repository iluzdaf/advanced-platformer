#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "content/item_catalog.hpp"
#include "level/level_composition.hpp"
#include "level/level_reload.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/inventory/inventory.hpp"
#include "advanced_platformer/inventory/item.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/world/pickup.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using Placed = std::vector<std::pair<std::string, advanced_platformer::Actor>>;
    using PlacedPickups = std::vector<std::pair<std::string, advanced_platformer::Pickup>>;

    const glm::vec2 SpawnFeet{8.0F, 48.0F};

    std::vector<advanced_platformer::ItemDefinition> items()
    {
        return {{1, "Coin", {}, 5}, {2, "Key", {}, 1}};
    }

    advanced_platformer::Actor player(
        int health = 3,
        int maximum = 3,
        advanced_platformer::Inventory inventory = advanced_platformer::Inventory(4))
    {
        return tests::ActorBuilder::sized({8.0F, 16.0F})
            .atFeet(SpawnFeet)
            .platforming()
            .withHealth(health, maximum)
            .withInventory(std::move(inventory));
    }

    advanced_platformer::Health healthOf(const advanced_platformer::Actor& actor)
    {
        return actor.health.value_or(advanced_platformer::Health{0, 0});
    }

    std::string placementOf(
        const advanced_platformer::GameLevel& level,
        const advanced_platformer::Pickup& pickup)
    {
        if (!pickup.placement.has_value())
        {
            return {};
        }
        return level.pickupPlacementIds.at(*pickup.placement);
    }

    advanced_platformer::Actor guard(
        glm::vec2 feet,
        int health = 3,
        advanced_platformer::PlatformerMovementConfig movement = {})
    {
        return tests::ActorBuilder::sized({8.0F, 12.0F})
            .atFeet(feet)
            .platforming(movement)
            .withHealth(health, health);
    }

    advanced_platformer::Pickup coin(glm::vec2 feet)
    {
        advanced_platformer::Pickup pickup;
        pickup.body.bounds = advanced_platformer::boxStandingOn(feet, {8.0F, 8.0F});
        pickup.stack = {1, 1};
        return pickup;
    }

    advanced_platformer::GameLevel level(
        advanced_platformer::Actor playerActor,
        Placed actors = {},
        PlacedPickups pickups = {},
        std::vector<advanced_platformer::ItemDefinition> definitions = items())
    {
        advanced_platformer::GameLevel result{
            .number = 1,
            .map = tests::TileMapBuilder({"........", "........", "..g.....", "########"})
                       .where('g', tests::Tile().blocksMovement().breaksInto('.')),
            .world = advanced_platformer::World(std::move(definitions)),
            .playerSpawnFeet = SpawnFeet,
            .actorPlacementIds = {},
            .pickupPlacementIds = {},
            .placedIds = {}};
        const advanced_platformer::ActorId playerId = result.world.addActor(std::move(playerActor));
        result.world.setPlayer(playerId, SpawnFeet);
        for (auto& [placement, actor] : actors)
        {
            const advanced_platformer::ActorId id = result.world.addActor(std::move(actor));
            result.actorPlacementIds.emplace(id.value, placement);
            result.placedIds.insert(placement);
        }
        for (auto& [placement, pickup] : pickups)
        {
            pickup.placement = result.pickupPlacementIds.size();
            result.pickupPlacementIds.push_back(placement);
            result.placedIds.insert(placement);
            result.world.addPickup(pickup);
        }
        return result;
    }

    std::map<advanced_platformer::ItemId, advanced_platformer::ItemId> sameItems()
    {
        return {{1, 1}, {2, 2}};
    }

    const advanced_platformer::Actor* placed(
        const advanced_platformer::GameLevel& level,
        const std::string& placement)
    {
        for (const auto& [actor, id] : level.actorPlacementIds)
        {
            if (id == placement)
            {
                return level.world.findActor({actor});
            }
        }
        return nullptr;
    }

    const advanced_platformer::Actor& playerOf(const advanced_platformer::GameLevel& level)
    {
        const advanced_platformer::Actor* actor = level.world.findActor(level.world.playerId());
        REQUIRE(actor != nullptr);
        return *actor;
    }
}

TEST_CASE("A kept actor stays where it is and takes its new definition", "[app][reload]")
{
    advanced_platformer::Actor moved = guard({40.0F, 32.0F});
    moved.body.velocity = {12.0F, -30.0F};
    moved.facing = advanced_platformer::Facing::Left;
    auto live = level(player(), {{"guard_1", std::move(moved)}});
    const advanced_platformer::ActorId id = placed(live, "guard_1")->id;

    const auto reload = advanced_platformer::reloadLevel(
        live,
        level(player(), {{"guard_1", guard({100.0F, 48.0F}, 2, {.maximumSpeed = 99.0F})}}),
        sameItems());

    const advanced_platformer::Actor* actor = placed(live, "guard_1");
    REQUIRE(actor != nullptr);
    REQUIRE(actor->id == id);
    REQUIRE(advanced_platformer::feetOf(actor->body.bounds) == glm::vec2{40.0F, 32.0F});
    REQUIRE(actor->body.velocity == glm::vec2{12.0F, -30.0F});
    REQUIRE(actor->facing == advanced_platformer::Facing::Left);
    REQUIRE(
        actor->platformerMovement.value_or(advanced_platformer::PlatformerMovement{})
            .config.maximumSpeed == 99.0F);
    REQUIRE(healthOf(*actor).maximum == 2);
    REQUIRE(healthOf(*actor).current == 2);
    REQUIRE(reload.kept == 1);
    REQUIRE(reload.spawned.empty());
    REQUIRE(reload.removed.empty());
}

TEST_CASE("A reload keeps damage taken but never heals", "[app][reload]")
{
    auto live = level(player(1, 3));

    advanced_platformer::reloadLevel(live, level(player(5, 5)), sameItems());

    REQUIRE(healthOf(playerOf(live)).current == 1);
    REQUIRE(healthOf(playerOf(live)).maximum == 5);
}

TEST_CASE("A placement gone from the file removes its actor", "[app][reload]")
{
    auto live = level(player(), {{"guard_1", guard({40.0F, 48.0F})}});
    const advanced_platformer::ActorId id = placed(live, "guard_1")->id;

    const auto reload = advanced_platformer::reloadLevel(live, level(player()), sameItems());

    REQUIRE(live.world.findActor(id) == nullptr);
    REQUIRE(live.actorPlacementIds.empty());
    REQUIRE(reload.removed == std::vector<std::string>{"guard_1"});
}

TEST_CASE("A new placement spawns where the file puts it", "[app][reload]")
{
    auto live = level(player());

    const auto reload = advanced_platformer::reloadLevel(
        live, level(player(), {{"guard_2", guard({72.0F, 48.0F})}}), sameItems());

    const advanced_platformer::Actor* actor = placed(live, "guard_2");
    REQUIRE(actor != nullptr);
    REQUIRE(advanced_platformer::feetOf(actor->body.bounds) == glm::vec2{72.0F, 48.0F});
    REQUIRE(reload.spawned == std::vector<std::string>{"guard_2"});
}

TEST_CASE("A killed actor stays dead while its placement remains", "[app][reload]")
{
    auto live = level(player(), {{"guard_1", guard({40.0F, 48.0F})}});
    live.world.removeActor(placed(live, "guard_1")->id);

    const auto reload = advanced_platformer::reloadLevel(
        live, level(player(), {{"guard_1", guard({40.0F, 48.0F})}}), sameItems());

    REQUIRE(placed(live, "guard_1") == nullptr);
    REQUIRE(reload.spawned.empty());
}

TEST_CASE("A placement taken out and put back spawns again", "[app][reload]")
{
    auto live = level(player(), {{"guard_1", guard({40.0F, 48.0F})}});
    live.world.removeActor(placed(live, "guard_1")->id);

    advanced_platformer::reloadLevel(live, level(player()), sameItems());
    const auto reload = advanced_platformer::reloadLevel(
        live, level(player(), {{"guard_1", guard({40.0F, 48.0F})}}), sameItems());

    REQUIRE(placed(live, "guard_1") != nullptr);
    REQUIRE(reload.spawned == std::vector<std::string>{"guard_1"});
}

TEST_CASE("Pickups are kept, collected, removed and spawned by placement", "[app][reload]")
{
    auto live = level(
        player(),
        {},
        {{"kept", coin({24.0F, 48.0F})},
         {"collected", coin({40.0F, 48.0F})},
         {"dropped", coin({56.0F, 48.0F})}});
    live.world.pickups()[0].body.bounds =
        advanced_platformer::boxStandingOn({30.0F, 20.0F}, {8.0F, 8.0F});
    live.world.collectPickup(1);

    const auto reload = advanced_platformer::reloadLevel(
        live,
        level(
            player(),
            {},
            {{"added", coin({88.0F, 48.0F})},
             {"kept", coin({24.0F, 48.0F})},
             {"collected", coin({40.0F, 48.0F})}}),
        sameItems());

    const auto& pickups = live.world.pickups();
    REQUIRE(pickups.size() == 2);
    REQUIRE(advanced_platformer::feetOf(pickups[0].body.bounds) == glm::vec2{30.0F, 20.0F});
    REQUIRE(placementOf(live, pickups[0]) == "kept");
    REQUIRE(placementOf(live, pickups[1]) == "added");
    REQUIRE(reload.kept == 1);
    REQUIRE(reload.spawned == std::vector<std::string>{"added"});
    REQUIRE(reload.removed == std::vector<std::string>{"dropped"});
}

TEST_CASE("Carried items follow their names to new item ids", "[app][reload]")
{
    advanced_platformer::ItemCatalog before;
    before.definitions = {{"coin", {1, "Coin", {}, 5}}, {"key", {2, "Key", {}, 1}}};
    advanced_platformer::ItemCatalog after;
    after.definitions = {
        {"bomb", {1, "Bomb", {}, 1}}, {"gem", {2, "Gem", {}, 1}}, {"key", {3, "Key", {}, 1}}};
    advanced_platformer::Inventory carried(4);
    carried.add({2, "Key", {}, 1}, 1);
    carried.add({1, "Coin", {}, 5}, 3);
    auto live = level(player(3, 3, std::move(carried)));

    advanced_platformer::reloadLevel(
        live,
        level(player(), {}, {}, {{1, "Bomb", {}, 1}, {2, "Gem", {}, 1}, {3, "Key", {}, 1}}),
        advanced_platformer::matchItemIds(before, after));

    const advanced_platformer::Inventory inventory =
        playerOf(live).inventory.value_or(advanced_platformer::Inventory(0));
    REQUIRE(inventory.count(3) == 1);
    REQUIRE(inventory.count(1) == 0);
    REQUIRE(inventory.count(2) == 0);
}

TEST_CASE("Tiles broken in play stay broken in the reloaded map", "[app][reload]")
{
    auto live = level(player());
    REQUIRE(live.map.breakTile({2, 2}));

    advanced_platformer::reloadLevel(live, level(player()), sameItems());

    REQUIRE_FALSE(live.map.blocksMovement({2, 2}));
}

TEST_CASE("A machine resumes in the state of the same name", "[app][reload]")
{
    const advanced_platformer::NpcStateMachine before{
        "guard", {{"rest", {"test", "idle"}}, {"hunt", {"test", "chase"}}}, {}};
    advanced_platformer::NpcStateMachine after = before;
    std::swap(after.states[0], after.states[1]);
    after.states.push_back({"flee", {"test", "flee"}});

    advanced_platformer::NpcMachine huntingMachine = advanced_platformer::startNpcMachine(before);
    huntingMachine.active = 1;
    huntingMachine.stateElapsed = 0.75F;
    huntingMachine.activityEntered = true;
    advanced_platformer::Actor hunting = tests::ActorBuilder::sized({8.0F, 12.0F})
                                             .atFeet({40.0F, 48.0F})
                                             .platforming()
                                             .thinking({})
                                             .running(before);
    hunting.machine = huntingMachine;
    auto live = level(player(), {{"guard_1", std::move(hunting)}});
    advanced_platformer::Actor rebuilt = tests::ActorBuilder::sized({8.0F, 12.0F})
                                             .atFeet({40.0F, 48.0F})
                                             .platforming()
                                             .thinking({})
                                             .running(after);

    advanced_platformer::reloadLevel(
        live, level(player(), {{"guard_1", std::move(rebuilt)}}), sameItems());

    const advanced_platformer::NpcMachine machine =
        placed(live, "guard_1")->machine.value_or(advanced_platformer::NpcMachine{});
    REQUIRE(advanced_platformer::activeNpcMachineState(machine).name == "hunt");
    REQUIRE(machine.stateElapsed == 0.75F);
    REQUIRE_FALSE(machine.activityEntered);
    REQUIRE(machine.definition.states.size() == 3);
}

TEST_CASE("A reload summary names what spawned and what was removed", "[app][reload]")
{
    REQUIRE(advanced_platformer::describeReload({4, {}, {}}) == "Reloaded content: kept 4");
    REQUIRE(
        advanced_platformer::describeReload({2, {"bat_3", "key_1"}, {"rat_1"}}) ==
        "Reloaded content: kept 2, spawned bat_3, key_1, removed rat_1");
}
