#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

#include "content/actor_catalog.hpp"
#include "content/actor_definition.hpp"
#include "content/machine_catalog.hpp"
#include "advanced_platformer/inventory/inventory.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/movement/flying_movement.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "support/actor_components.hpp"

TEST_CASE("Ranged definitions create fresh weapons with runtime texture IDs", "[app][actors]")
{
    const auto catalog = advanced_platformer::parseActorCatalog(
        R"({
      "player":"hero", "actors":{"hero":{"bodySize":[12,20],"health":4,"inventorySlots":2,
      "platformer":{}, "team":"player", "primaryAttack":{"kind":"ranged","damage":2,"projectileSize":[3,2],
      "projectileSpeed":120,"projectileLifetime":0.6,"shootDuration":0.2,"recoveryDuration":0.8,
      "sprite": {"position": [4,8], "size": [8,4]}}}}})",
        "weapons",
        {});
    auto definition = advanced_platformer::actorDefinition(catalog, "hero");
    auto* weapon = definition.primaryAttack.has_value()
                       ? std::get_if<advanced_platformer::RangedWeapon>(&*definition.primaryAttack)
                       : nullptr;
    if (weapon == nullptr)
    {
        throw std::logic_error("Weapon was not parsed");
    }
    weapon->phase = advanced_platformer::RangedPhase::Recovery;
    weapon->lastFiredTimeSeconds = 3.0;
    auto actor = advanced_platformer::composeActor(definition, {}, 9);
    REQUIRE(tests::component<advanced_platformer::RangedWeapon>(actor).damage == 2);
    REQUIRE(tests::component<advanced_platformer::RangedWeapon>(actor).projectileSpeed == 120);
    REQUIRE(
        tests::component<advanced_platformer::RangedWeapon>(actor).projectileSprite.textureId == 9);
    REQUIRE(
        tests::component<advanced_platformer::RangedWeapon>(actor)
            .projectileSprite.region.position.x == 4);
    REQUIRE(
        tests::component<advanced_platformer::RangedWeapon>(actor).phase ==
        advanced_platformer::RangedPhase::Ready);
    REQUIRE_FALSE(
        tests::component<advanced_platformer::RangedWeapon>(actor)
            .lastFiredTimeSeconds.has_value());
}

TEST_CASE("Contact damage definitions compose fresh independent state", "[app][actors][contact]")
{
    const auto catalog = advanced_platformer::parseActorCatalog(
        R"({"player":"runner","actors":{"runner":{"bodySize":[12,12],"team":"player",
             "health":3,"inventorySlots":1,"platformer":{},
             "primaryAttack":{"kind":"contact","damage":2}}}})",
        "contact damage",
        {});
    auto definition = advanced_platformer::actorDefinition(catalog, "runner");
    auto* contactDamage =
        definition.primaryAttack.has_value()
            ? std::get_if<advanced_platformer::ContactDamage>(&*definition.primaryAttack)
            : nullptr;
    if (contactDamage == nullptr)
    {
        throw std::logic_error("Contact damage was not parsed");
    }
    contactDamage->active = true;
    contactDamage->actorsHit.push_back(advanced_platformer::ActorId{7});

    auto composed = advanced_platformer::composeActor(definition, {}, 0);
    REQUIRE(tests::component<advanced_platformer::ContactDamage>(composed).damage == 2);
    REQUIRE_FALSE(tests::component<advanced_platformer::ContactDamage>(composed).active);
    REQUIRE(tests::component<advanced_platformer::ContactDamage>(composed).actorsHit.empty());
}

TEST_CASE("Contact damage knockback is read with defaults and validated", "[app][actors][contact]")
{
    const auto heroWith = [](const char* fields)
    {
        const auto catalog = advanced_platformer::parseActorCatalog(
            std::string(R"({"player":"hero","actors":{"hero":{"bodySize":[12,20],"team":"player",
                 "health":3,"inventorySlots":1,"platformer":{},
                 "primaryAttack":{"kind":"contact")") +
                fields + "}}}}",
            "knockback",
            {});
        return advanced_platformer::composeActor(
            advanced_platformer::actorDefinition(catalog, "hero"), {}, 0);
    };

    advanced_platformer::Actor partlyConfigured = heroWith(R"(,"knockback":{"speed":180})");
    const advanced_platformer::Knockback knockback =
        tests::component<advanced_platformer::ContactDamage>(partlyConfigured)
            .knockback.value_or(advanced_platformer::Knockback{0.0F, 0.0F});
    REQUIRE(knockback.speed == 180.0F);
    REQUIRE(knockback.lift == 120.0F);

    advanced_platformer::Actor plain = heroWith("");
    REQUIRE_FALSE(
        tests::component<advanced_platformer::ContactDamage>(plain).knockback.has_value());

    REQUIRE_THROWS_AS(heroWith(R"(,"knockback":{"speed":-1})"), std::invalid_argument);
}

TEST_CASE(
    "Contact damage shares an actor with another attack and either movement",
    "[app][actors][contact]")
{
    advanced_platformer::ActorDefinition definition;
    definition.bodySize = {12.0F, 12.0F};
    definition.team = advanced_platformer::Team::Enemy;
    definition.secondaryAttack = advanced_platformer::ContactDamage{};
    SECTION("Walking with a bite")
    {
        definition.platformer = advanced_platformer::PlatformerMovementConfig{};
        definition.primaryAttack = advanced_platformer::BiteAttack{};
    }
    SECTION("Flying with a ranged weapon")
    {
        definition.flying = advanced_platformer::FlyingMovement{};
        definition.primaryAttack = advanced_platformer::RangedWeapon{};
    }
    REQUIRE_NOTHROW(advanced_platformer::validateActorDefinition(definition, {}));
}

TEST_CASE("Actor composition creates fresh independent runtime state", "[app][actors]")
{
    advanced_platformer::ActorDefinition definition;
    definition.bodySize = {12.0F, 20.0F};
    definition.platformer = advanced_platformer::PlatformerMovementConfig{};
    definition.platformer.value().maximumSpeed = 42;
    definition.team = advanced_platformer::Team::Enemy;
    definition.senses = advanced_platformer::NpcSenses{70, 2};
    definition.health = 4;
    definition.inventorySlots = 2;
    advanced_platformer::BiteAttack usedBite;
    usedBite.phase = advanced_platformer::BitePhase::Recovery;
    usedBite.phaseTimeRemaining = 10;
    definition.primaryAttack = usedBite;
    definition.machine = "test_machine";
    const auto machines =
        advanced_platformer::loadMachineCatalog("tests/fixtures/catalogs/machines.json");
    auto first = advanced_platformer::composeActor(
        definition,
        {},
        0,
        {24, 32},
        advanced_platformer::Patrol{{8, 32}, {40, 32}, true},
        machines);
    auto second =
        advanced_platformer::composeActor(definition, {}, 0, {40, 32}, std::nullopt, machines);
    REQUIRE(
        tests::component<advanced_platformer::PlatformerMovement>(first).config.maximumSpeed == 42);
    REQUIRE(advanced_platformer::feetOf(first.body.bounds).x == 24);
    REQUIRE(first.brain.has_value());
    REQUIRE_FALSE(tests::component<advanced_platformer::NpcPerception>(first).targetVisible);
    REQUIRE_FALSE(tests::component<advanced_platformer::NpcPerception>(first).heardLanding);
    tests::component<advanced_platformer::NpcPerception>(first).targetVisible = true;
    tests::component<advanced_platformer::NpcPerception>(first).heardLanding = true;
    REQUIRE_FALSE(tests::component<advanced_platformer::NpcPerception>(second).targetVisible);
    REQUIRE_FALSE(tests::component<advanced_platformer::NpcPerception>(second).heardLanding);
    REQUIRE(first.pathFollower.has_value());
    REQUIRE(first.patrol.has_value());
    REQUIRE_FALSE(second.patrol.has_value());
    REQUIRE(
        tests::component<advanced_platformer::BiteAttack>(first).phase ==
        advanced_platformer::BitePhase::Ready);
    REQUIRE(tests::component<advanced_platformer::BiteAttack>(first).phaseTimeRemaining == 0);
    tests::component<advanced_platformer::Health>(second).current = 1;
    REQUIRE(tests::component<advanced_platformer::Health>(first).current == 4);
    REQUIRE(tests::component<advanced_platformer::Inventory>(first).slots().size() == 2);
}

TEST_CASE("Actor definitions reuse engine component validation", "[app][actors]")
{
    advanced_platformer::ActorDefinition definition;
    definition.bodySize = {12.0F, 20.0F};
    definition.platformer = advanced_platformer::PlatformerMovementConfig{};
    SECTION("Two movements")
    {
        definition.flying = advanced_platformer::FlyingMovement{};
    }
    SECTION("Invalid body")
    {
        definition.bodySize.x = 0;
    }
    SECTION("Invalid health")
    {
        definition.health = 0;
    }
    SECTION("Invalid senses")
    {
        definition.senses = advanced_platformer::NpcSenses{-1, 1};
        definition.machine = "test_machine";
    }
    SECTION("Negative movement")
    {
        definition.platformer.value().maximumSpeed = -1;
    }
    SECTION("Invalid inventory")
    {
        definition.inventorySlots = 0;
    }
    SECTION("Neutral attacker")
    {
        definition.primaryAttack = advanced_platformer::BiteAttack{};
    }
    SECTION("Invalid contact damage")
    {
        definition.team = advanced_platformer::Team::Enemy;
        definition.primaryAttack = advanced_platformer::ContactDamage{.damage = 0};
    }
    SECTION("Neutral contact damage")
    {
        definition.primaryAttack = advanced_platformer::ContactDamage{};
    }
    SECTION("Senses without a machine")
    {
        definition.senses = advanced_platformer::NpcSenses{};
    }
    SECTION("A negative standoff")
    {
        definition.senses = advanced_platformer::NpcSenses{};
        definition.senses->standoffDistance = -1.0F;
        definition.machine = "test_machine";
    }
    SECTION("A machine without senses")
    {
        definition.machine = "test_machine";
    }
    SECTION("A machine the catalog lacks")
    {
        definition.senses = advanced_platformer::NpcSenses{};
        definition.machine = "missing";
    }
    REQUIRE_THROWS_AS(
        advanced_platformer::validateActorDefinition(
            definition,
            {},
            advanced_platformer::loadMachineCatalog("tests/fixtures/catalogs/machines.json")),
        std::invalid_argument);
}
