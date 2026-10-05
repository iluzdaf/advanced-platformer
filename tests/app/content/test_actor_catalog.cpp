#include <catch2/catch_test_macros.hpp>

#include <variant>

#include <catch2/matchers/catch_matchers_string.hpp>
#include <string>

#include <optional>
#include <stdexcept>

#include "content/actor_catalog.hpp"
#include "advanced_platformer/movement/flying_movement.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/movement/pounce.hpp"
#include "content/actor_definition.hpp"
#include "content/animation_catalog.hpp"
#include "content/machine_catalog.hpp"
#include "advanced_platformer/combat/combat.hpp"
#include "advanced_platformer/movement/platformer_movement.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/render/sprite.hpp"
#include "support/actor_components.hpp"
#include "support/json_document.hpp"

TEST_CASE("Actor JSON accepts custom names and configures component choices", "[app][actors][json]")
{
    const auto animations =
        advanced_platformer::loadAnimationCatalog("tests/fixtures/catalogs/animations.json");
    const auto machines =
        advanced_platformer::loadMachineCatalog("tests/fixtures/catalogs/machines.json");
    const auto catalog = advanced_platformer::parseActorCatalog(
        R"({
        "player":"hero", "actors":{
          "hero":{"bodySize":[12,20],"movement":{"platformer":{"jumpSpeed":210}},"health":5,"inventorySlots":3},
          "scout":{"movement":{"flying":{"speed":25}},"team":"enemy","senses":{"noticeDistance":40,"searchDuration":3,"standoffDistance":30},
                   "machine":"test_machine",
                   "bodySize":[8,6],"animations":"test_actor","spriteAnchor":"center",
                   "primaryAttack":{"bite":{"damage":2}}}
        }})",
        "test actors",
        animations,
        machines);
    REQUIRE(catalog.player == "hero");
    auto actor = advanced_platformer::composeActor(
        advanced_platformer::actorDefinition(catalog, "scout"),
        animations,
        7,
        {},
        std::nullopt,
        machines);
    REQUIRE(actor.machine.has_value());
    REQUIRE(
        advanced_platformer::activeNpcMachineState(
            actor.machine.value_or(advanced_platformer::NpcMachine{}))
            .name == "rest");
    REQUIRE(tests::component<advanced_platformer::FlyingMovement>(actor).config.speed == 25);
    REQUIRE(tests::component<advanced_platformer::BiteAttack>(actor).damage == 2);
    REQUIRE(tests::component<advanced_platformer::NpcSenses>(actor).searchDuration == 3);
    REQUIRE(tests::component<advanced_platformer::NpcSenses>(actor).standoffDistance == 30);
    REQUIRE(tests::component<advanced_platformer::Sprite>(actor).textureId == 7);
    REQUIRE(
        tests::component<advanced_platformer::Sprite>(actor).anchor ==
        advanced_platformer::SpriteAnchor::BodyCenter);
    REQUIRE_FALSE(actor.platformerMovement.has_value());
    REQUIRE_THROWS_AS(
        advanced_platformer::actorDefinition(catalog, "missing"), std::invalid_argument);
}

TEST_CASE("Actor JSON configures climbing without exposing attachment state", "[app][actors][json]")
{
    const auto catalog = advanced_platformer::parseActorCatalog(
        R"({"player":"hero","actors":{"hero":{"bodySize":[12,12],"movement":{"platformer":{}},
        "surfaceClimb":{"speed":75},"health":2,"inventorySlots":1}}})",
        "actors.json",
        {});

    auto actor = advanced_platformer::composeActor(
        advanced_platformer::actorDefinition(catalog, "hero"), {}, 0);
    REQUIRE(tests::component<advanced_platformer::SurfaceClimb>(actor).config.speed == 75.0F);
    REQUIRE(
        tests::component<advanced_platformer::SurfaceClimb>(actor).surface ==
        advanced_platformer::ClimbSurface::None);
}

TEST_CASE("Climbing requires platformer movement and positive speed", "[app][actors][json]")
{
    auto actorJson = tests::parseJson(
        R"({"player":"hero","actors":{"hero":{"bodySize":[12,12],"movement":{"platformer":{}},
        "surfaceClimb":{"speed":75},"health":2,"inventorySlots":1}}})");
    SECTION("Flying actor")
    {
        actorJson["actors"]["hero"]["movement"] =
            tests::object({{"flying", tests::object({{"speed", 60}})}});
    }
    SECTION("Invalid speed")
    {
        actorJson["actors"]["hero"]["surfaceClimb"]["speed"] = 0;
    }
    SECTION("Runtime attachment state")
    {
        actorJson["actors"]["hero"]["surfaceClimb"]["surface"] = "ceiling";
        REQUIRE_THROWS_WITH(
            advanced_platformer::parseActorCatalog(tests::dumpJson(actorJson), "actors.json", {}),
            Catch::Matchers::EndsWith("unknown field 'surface'"));
    }

    REQUIRE_THROWS_WITH(
        advanced_platformer::parseActorCatalog(tests::dumpJson(actorJson), "actors.json", {}),
        Catch::Matchers::ContainsSubstring("actors.json:"));
}

TEST_CASE("An attack takes only its kind's fields", "[app][actors][json]")
{
    auto actorJson = tests::parseJson(
        R"({"player":"hero","actors":{"hero":{"bodySize":[12,12],"movement":{"platformer":{}},"team":"enemy",
        "primaryAttack":{"bite":{}},"health":2,"inventorySlots":1}}})");
    const char* expected = "";
    SECTION("A bite with a projectile speed")
    {
        actorJson["actors"]["hero"]["primaryAttack"]["bite"]["projectileSpeed"] = 100;
        expected = "unknown field 'projectileSpeed'";
    }
    SECTION("A contact attack with a reach")
    {
        actorJson["actors"]["hero"]["secondaryAttack"] =
            tests::object({{"contact", tests::object({{"reach", 4}})}});
        expected = "unknown field 'reach'";
    }
    SECTION("An unknown attack")
    {
        actorJson["actors"]["hero"]["primaryAttack"] =
            tests::object({{"laser", tests::emptyObject()}});
        expected = "unknown field 'laser'";
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseActorCatalog(tests::dumpJson(actorJson), "actors.json", {}),
        Catch::Matchers::StartsWith("actors.json: line 1, column ") &&
            Catch::Matchers::EndsWith(expected));
}

TEST_CASE("A pounce requires platformer movement and valid settings", "[app][actors][json]")
{
    auto actorJson = tests::parseJson(
        R"({"player":"hero","actors":{"hero":{"bodySize":[12,12],"movement":{"platformer":{}},"team":"enemy",
        "primaryAttack":{"pounce":{"speed":220}},"health":2,"inventorySlots":1}}})");
    SECTION("Flying actor")
    {
        actorJson["actors"]["hero"]["movement"] =
            tests::object({{"flying", tests::object({{"speed", 60}})}});
    }
    SECTION("Invalid speed")
    {
        actorJson["actors"]["hero"]["primaryAttack"]["pounce"]["speed"] = 0;
    }
    SECTION("Runtime phase")
    {
        actorJson["actors"]["hero"]["primaryAttack"]["pounce"]["phase"] = "airborne";
        REQUIRE_THROWS_WITH(
            advanced_platformer::parseActorCatalog(tests::dumpJson(actorJson), "actors.json", {}),
            Catch::Matchers::EndsWith("unknown field 'phase'"));
    }

    REQUIRE_THROWS_WITH(
        advanced_platformer::parseActorCatalog(tests::dumpJson(actorJson), "actors.json", {}),
        Catch::Matchers::ContainsSubstring("actors.json:"));
}

TEST_CASE("Pounce settings keep the C++ defaults a file leaves out", "[app][actors][json]")
{
    const auto catalog = advanced_platformer::parseActorCatalog(
        R"({"player":"hero","actors":{"hero":{"bodySize":[12,12],"movement":{"platformer":{}},"team":"enemy",
        "primaryAttack":{"pounce":{"speed":220}},"health":2,"inventorySlots":1}}})",
        "actors.json",
        {});

    const advanced_platformer::PounceConfig pounce =
        std::get<advanced_platformer::Pounce>(
            advanced_platformer::actorDefinition(catalog, "hero")
                .primaryAttack.value_or(advanced_platformer::Pounce{}))
            .config;

    REQUIRE(pounce.speed == 220.0F);
    REQUIRE(pounce.lift == 120.0F);
    REQUIRE(pounce.range == 64.0F);
    REQUIRE(pounce.recoveryDuration == 0.5F);
}

TEST_CASE(
    "Actor JSON rejects malformed and invalid definitions including unused ones",
    "[app][actors][json]")
{
    auto actorJson = tests::parseJson(
        R"({"player":"hero","actors":{"hero":{"bodySize":[12,20],"movement":{"platformer":{}},"health":3,"inventorySlots":2}}})");
    std::string start = "actors.json: actors.";
    std::string end;
    SECTION("Missing body size")
    {
        tests::eraseKey(actorJson["actors"]["hero"], "bodySize");
        start = "actors.json: line 1, column ";
        end = "missing 'bodySize'";
    }
    SECTION("Missing player reference")
    {
        actorJson["player"] = "missing";
        start = "actors.json: ";
    }
    SECTION("Unknown animation")
    {
        actorJson["actors"]["hero"]["animations"] = "missing";
    }
    SECTION("Unused actor references an unknown animation")
    {
        actorJson["actors"]["unused"] = tests::object(
            {{"bodySize", tests::numbers({8, 8})},
             {"movement", tests::object({{"flying", tests::object({{"speed", 25}})}})},
             {"animations", "missing"}});
    }
    SECTION("Fractional health")
    {
        actorJson["actors"]["hero"]["health"] = 1.5;
        start = "actors.json: line 1, column ";
        end = "invalid number '1.5'";
    }
    SECTION("Boolean speed")
    {
        actorJson["actors"]["hero"]["movement"]["platformer"]["maximumSpeed"] = true;
        start = "actors.json: line 1, column ";
        end = "invalid number 'true'";
    }
    SECTION("Unknown field")
    {
        actorJson["actors"]["hero"]["heath"] = 3;
        start = "actors.json: line 1, column ";
        end = "unknown field 'heath'";
    }
    SECTION("Runtime state")
    {
        actorJson["actors"]["hero"]["brain"] = tests::emptyObject();
        start = "actors.json: line 1, column ";
        end = "unknown field 'brain'";
    }
    SECTION("Runtime attack state")
    {
        actorJson["actors"]["hero"]["primaryAttack"] =
            tests::object({{"bite", tests::object({{"phase", "active"}})}});
        start = "actors.json: line 1, column ";
        end = "unknown field 'phase'";
    }
    SECTION("Unknown team")
    {
        actorJson["actors"]["hero"]["team"] = "pirates";
        start = "actors.json: line 1, column ";
        end = "unknown value 'pirates'";
    }
    SECTION("Unused definition")
    {
        actorJson["actors"]["unused"] = tests::object(
            {{"bodySize", tests::numbers({8, 8})},
             {"movement", tests::object({{"flying", tests::object({{"speed", -1}})}})}});
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseActorCatalog(tests::dumpJson(actorJson), "actors.json", {}),
        Catch::Matchers::StartsWith(start) && Catch::Matchers::EndsWith(end));
}

TEST_CASE("Actor JSON keeps the C++ defaults a component leaves out", "[app][actors][json]")
{
    const auto catalog = advanced_platformer::parseActorCatalog(
        R"({"player":"hero","actors":{
        "hero":{"bodySize":[12,20],"movement":{"platformer":{}},"health":3,"inventorySlots":2},
        "guard":{"bodySize":[12,20],"team":"enemy","health":2,
        "movement":{"platformer":{"maximumSpeed":60}},"senses":{"noticeDistance":80},"machine":"test_machine",
        "primaryAttack":{"bite":{"damage":2}}}}})",
        "actors.json",
        {},
        advanced_platformer::loadMachineCatalog("tests/fixtures/catalogs/machines.json"));
    const auto& guard = advanced_platformer::actorDefinition(catalog, "guard");
    const advanced_platformer::PlatformerMovementConfig movementDefaults;
    const advanced_platformer::NpcSenses sensesDefaults;
    const advanced_platformer::BiteAttack biteDefaults;
    const advanced_platformer::PlatformerMovementConfig guardMovement =
        std::get<advanced_platformer::PlatformerMovementConfig>(guard.movement);
    REQUIRE(guardMovement.maximumSpeed == 60.0F);
    REQUIRE(guardMovement.gravity == movementDefaults.gravity);
    REQUIRE(guard.senses.value_or(sensesDefaults).noticeDistance == 80.0F);
    REQUIRE(guard.senses.value_or(sensesDefaults).searchDuration == sensesDefaults.searchDuration);
    const advanced_platformer::BiteAttack guardBite =
        std::get<advanced_platformer::BiteAttack>(guard.primaryAttack.value_or(biteDefaults));
    REQUIRE(guardBite.damage == 2);
    REQUIRE(guardBite.reach == biteDefaults.reach);
    REQUIRE(std::holds_alternative<advanced_platformer::PlatformerMovementConfig>(guard.movement));
}
