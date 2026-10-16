#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <numbers>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/combat/attack_system.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/inventory/inventory.hpp"
#include "advanced_platformer/inventory/item.hpp"
#include "advanced_platformer/inventory/item_use.hpp"
#include "advanced_platformer/world/world.hpp"
#include "advanced_platformer/world/world_requests.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/require_near.hpp"

namespace
{
    constexpr int GunId = 1;
    constexpr int ShellsId = 2;

    advanced_platformer::RangedWeapon spreadGun()
    {
        advanced_platformer::RangedWeapon gun;
        gun.projectilesPerShot = 5;
        gun.spreadDegrees = 40.0F;
        return gun;
    }

    advanced_platformer::World makeWorld(int shells)
    {
        advanced_platformer::World world(
            {{.id = GunId,
              .name = "Spread gun",
              .maximumStack = 1,
              .weapon = spreadGun(),
              .ammo = ShellsId},
             {.id = ShellsId, .name = "Shells", .maximumStack = 24}});
        advanced_platformer::Inventory bag(3);
        bag.add(world.itemDefinition(GunId), 1);
        if (shells > 0)
        {
            bag.add(world.itemDefinition(ShellsId), shells);
        }
        tests::addPlayer(
            world,
            tests::ActorBuilder::sized({12.0F, 12.0F})
                .atFeet({40.0F, 40.0F})
                .platforming()
                .withHealth(3, 3)
                .onTeam(advanced_platformer::Team::Player)
                .withInventory(bag)
                .withPrimary(advanced_platformer::RangedWeapon{}));
        return world;
    }

    int shotsPerPrimary(advanced_platformer::World& world)
    {
        return std::get<advanced_platformer::RangedWeapon>(tests::player(world).primaryAttack.value_or(advanced_platformer::BiteAttack{}))
            .projectilesPerShot;
    }

    void pullTrigger(advanced_platformer::World& world)
    {
        tests::player(world).intentions.aimDirection = {1.0F, 0.0F};
        tests::player(world).intentions.primaryAttackPressed = true;
        advanced_platformer::WorldRequests requests;
        advanced_platformer::updateAttacks(world, requests, 0.01F);
        advanced_platformer::applyWorldRequests(world, requests);
        tests::player(world).intentions.primaryAttackPressed = false;
        advanced_platformer::WorldRequests recovery;
        advanced_platformer::updateAttacks(world, recovery, 1.0F);
        advanced_platformer::applyWorldRequests(world, recovery);
    }
}

TEST_CASE(
    "Using a gun from the bag equips it, and using it again brings back the first attack",
    "[combat][weapon][inventory]")
{
    advanced_platformer::World world = makeWorld(4);

    REQUIRE(advanced_platformer::useItem(world, world.playerId(), 0));
    REQUIRE(tests::player(world).equippedWeapon.has_value());
    REQUIRE(tests::player(world).equippedWeapon.value_or(advanced_platformer::EquippedWeapon{}).item == GunId);
    REQUIRE(shotsPerPrimary(world) == 5);

    REQUIRE(advanced_platformer::useItem(world, world.playerId(), 0));
    REQUIRE_FALSE(tests::player(world).equippedWeapon.has_value());
    REQUIRE(shotsPerPrimary(world) == 1);
}

TEST_CASE(
    "Each shot of a gun spends one ammo, and the first attack returns when the ammo runs out",
    "[combat][weapon][inventory]")
{
    advanced_platformer::World world = makeWorld(2);
    REQUIRE(advanced_platformer::useItem(world, world.playerId(), 0));

    pullTrigger(world);
    REQUIRE(world.projectiles().size() == 5);
    REQUIRE(tests::component<advanced_platformer::Inventory>(tests::player(world)).count(ShellsId) == 1);
    REQUIRE(tests::player(world).equippedWeapon.has_value());

    pullTrigger(world);
    REQUIRE(tests::component<advanced_platformer::Inventory>(tests::player(world)).count(ShellsId) == 0);
    REQUIRE_FALSE(tests::player(world).equippedWeapon.has_value());
    REQUIRE(shotsPerPrimary(world) == 1);
    REQUIRE(tests::component<advanced_platformer::Inventory>(tests::player(world)).count(GunId) == 1);
}

TEST_CASE("A gun without its ammo in the bag cannot be equipped", "[combat][weapon][inventory]")
{
    advanced_platformer::World world = makeWorld(0);

    REQUIRE_FALSE(advanced_platformer::useItem(world, world.playerId(), 0));
    REQUIRE_FALSE(tests::player(world).equippedWeapon.has_value());
    REQUIRE(shotsPerPrimary(world) == 1);
}

TEST_CASE("A spread shot fans its projectiles evenly across the spread", "[combat][weapon]")
{
    advanced_platformer::World world = makeWorld(1);
    REQUIRE(advanced_platformer::useItem(world, world.playerId(), 0));

    pullTrigger(world);

    std::vector<float> angles;
    for (const advanced_platformer::Projectile& projectile : world.projectiles())
    {
        angles.push_back(std::atan2(projectile.velocity.y, projectile.velocity.x));
    }
    REQUIRE(angles.size() == 5);
    const float step = 10.0F * std::numbers::pi_v<float> / 180.0F;
    for (std::size_t index = 0; index < angles.size(); ++index)
    {
        REQUIRE_NEAR(angles[index], step * (static_cast<float>(index) - 2.0F));
    }
}

TEST_CASE("A weapon item needs another item as its ammo", "[combat][weapon][validation]")
{
    advanced_platformer::ItemDefinition gun{
        .id = GunId, .name = "Gun", .maximumStack = 1, .weapon = spreadGun(), .ammo = ShellsId};
    REQUIRE_NOTHROW(advanced_platformer::validateItemDefinition(gun));

    SECTION("No ammo")
    {
        gun.ammo.reset();
    }
    SECTION("Ammo without a weapon")
    {
        gun.weapon.reset();
    }
    SECTION("Its own ammo")
    {
        gun.ammo = GunId;
    }
    SECTION("Another effect")
    {
        gun.effect = advanced_platformer::ItemEffect::Heal;
        gun.effectAmount = 1;
    }
    SECTION("No projectiles per shot")
    {
        advanced_platformer::RangedWeapon weapon = spreadGun();
        weapon.projectilesPerShot = 0;
        gun.weapon = weapon;
    }
    SECTION("A spread of half a turn")
    {
        advanced_platformer::RangedWeapon weapon = spreadGun();
        weapon.spreadDegrees = 180.0F;
        gun.weapon = weapon;
    }
    REQUIRE_THROWS_AS(advanced_platformer::validateItemDefinition(gun), std::invalid_argument);
}
