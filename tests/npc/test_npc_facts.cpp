#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/movement/pounce.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/platformer_cells.hpp"
#include "advanced_platformer/navigation/route.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_facts.hpp"
#include "advanced_platformer/npc/npc_senses.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

using tests::actor;
using tests::brain;

namespace
{
    tests::ActorBuilder makePlayer(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F}).atFeet(feet).platforming();
    }

    advanced_platformer::ActorId addWalkingNpc(advanced_platformer::World& world)
    {
        const advanced_platformer::ActorId npcId = world.addActor(
            tests::ActorBuilder::sized({12.0F, 12.0F})
                .atFeet({24.0F, 32.0F})
                .platforming()
                .thinking({32.0F, 1.0F}));
        tests::platformerMovement(actor(world, npcId)).grounded = true;
        return npcId;
    }

    advanced_platformer::NpcFacts factsOf(
        const advanced_platformer::TileMap& map,
        advanced_platformer::World& world,
        advanced_platformer::ActorId npcId)
    {
        const advanced_platformer::NpcBrain& npcBrain = brain(world, npcId);
        return advanced_platformer::gatherNpcFacts(
            map,
            actor(world, npcId),
            npcBrain,
            tests::perception(world, npcId),
            advanced_platformer::livingTarget(world, npcBrain),
            0.0F);
    }
}

TEST_CASE("Same-surface and notice-distance facts are independent", "[npc][facts]")
{
    struct ExpectedFacts
    {
        bool sameSurface;
        bool withinNoticeDistance;
    };

    struct Scenario
    {
        std::string name;
        ExpectedFacts expected;
        std::string floor = "############";
        glm::vec2 targetFeet{56.0F, 32.0F};
        bool targetGrounded = true;
        bool rememberTarget = true;
        bool targetAlive = true;
    };

    const auto observeFacts = [](const Scenario& scenario)
    {
        const advanced_platformer::TileMap map =
            tests::TileMapBuilder({"............", "............", scenario.floor});
        advanced_platformer::World world;
        const auto targetId = world.addActor(makePlayer(scenario.targetFeet));
        tests::platformerMovement(actor(world, targetId)).grounded = scenario.targetGrounded;
        if (!scenario.targetAlive)
        {
            actor(world, targetId).life = advanced_platformer::LifeState::Dying;
        }
        const auto npcId = addWalkingNpc(world);
        if (scenario.rememberTarget)
        {
            brain(world, npcId).target = targetId;
        }
        brain(world, npcId).lastKnownTargetFeet = {24.0F, 32.0F};
        return factsOf(map, world, npcId);
    };

    constexpr ExpectedFacts SameSurfaceAndWithinNotice{true, true};
    constexpr ExpectedFacts SameSurfaceOnly{true, false};
    constexpr ExpectedFacts WithinNoticeOnly{false, true};
    constexpr ExpectedFacts Neither{false, false};
    std::vector<Scenario> scenarios{
        {"Same surface at the inclusive notice boundary", SameSurfaceAndWithinNotice}};

    Scenario beyondNotice{"Same surface beyond notice distance", SameSurfaceOnly};
    beyondNotice.targetFeet.x = 120.0F;
    scenarios.push_back(beyondNotice);

    Scenario nearbyGap{"Nearby across a gap", WithinNoticeOnly};
    nearbyGap.floor = "##.#########";
    scenarios.push_back(nearbyGap);

    Scenario distantGap{"Distant across a gap", Neither};
    distantGap.floor = "##.#########";
    distantGap.targetFeet.x = 120.0F;
    scenarios.push_back(distantGap);

    Scenario airborne{"Nearby but airborne", WithinNoticeOnly};
    airborne.targetGrounded = false;
    scenarios.push_back(airborne);

    Scenario forgotten{"No remembered target", Neither};
    forgotten.rememberTarget = false;
    scenarios.push_back(forgotten);

    Scenario dying{"A dead remembered target", Neither};
    dying.targetAlive = false;
    scenarios.push_back(dying);

    for (const Scenario& scenario : scenarios)
    {
        INFO(scenario.name);
        const auto facts = observeFacts(scenario);
        REQUIRE_FALSE(facts.targetVisible);
        REQUIRE(facts.targetOnSameSurface == scenario.expected.sameSurface);
        REQUIRE(facts.targetWithinNoticeDistance == scenario.expected.withinNoticeDistance);
    }
}

TEST_CASE("A climber's surface reaches its target along walls and ceilings", "[npc][facts]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"cccccccc", "c......c", "c..cc..c", "c......c", "########"})
            .where('c', tests::Tile().blocksMovement().climbable());
    advanced_platformer::RouteLocation climberAt{{2, 3}, advanced_platformer::ClimbSurface::None};
    bool climberGrounded = true;
    bool targetGrounded = true;
    bool expected = true;
    SECTION("Standing on the target's floor")
    {
    }
    SECTION("On a wall that comes down to the target's floor")
    {
        climberAt = {{1, 2}, advanced_platformer::ClimbSurface::LeftWall};
    }
    SECTION("On a ceiling joined to the target's floor by a wall")
    {
        climberAt = {{3, 1}, advanced_platformer::ClimbSurface::Ceiling};
    }
    SECTION("On the side of a block that touches nothing else")
    {
        climberAt = {{2, 2}, advanced_platformer::ClimbSurface::RightWall};
        expected = false;
    }
    SECTION("In the air")
    {
        climberGrounded = false;
        expected = false;
    }
    SECTION("Below an airborne target")
    {
        targetGrounded = false;
        expected = false;
    }

    advanced_platformer::World world;
    const auto targetId = world.addActor(makePlayer({88.0F, 64.0F}));
    tests::platformerMovement(actor(world, targetId)).grounded = targetGrounded;
    const glm::vec2 climberSize{8.0F, 8.0F};
    const advanced_platformer::Aabb bounds =
        advanced_platformer::boundsAtSurface(tests::TileSize, climberAt, climberSize);
    const auto npcId = world.addActor(
        tests::ActorBuilder::sized(climberSize)
            .at(bounds.topLeft)
            .platforming()
            .climbing()
            .thinking({32.0F, 1.0F}));
    tests::surfaceClimb(actor(world, npcId)).surface = climberAt.surface;
    tests::platformerMovement(actor(world, npcId)).grounded =
        climberGrounded && climberAt.surface == advanced_platformer::ClimbSurface::None;
    brain(world, npcId).target = targetId;

    REQUIRE(factsOf(map, world, npcId).targetOnSameSurface == expected);
}

TEST_CASE("Pounce facts follow the phase and the visible target's distance", "[npc][facts]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"............", "............", "############"});
    advanced_platformer::World world;
    glm::vec2 targetFeet{56.0F, 32.0F};
    bool visible = true;
    bool inRange = true;
    advanced_platformer::PouncePhase phase = advanced_platformer::PouncePhase::Ready;
    SECTION("A visible target within range")
    {
    }
    SECTION("A target beyond range")
    {
        targetFeet.x = 120.0F;
        inRange = false;
    }
    SECTION("A target out of sight")
    {
        visible = false;
        inRange = false;
    }
    SECTION("In the air")
    {
        phase = advanced_platformer::PouncePhase::Airborne;
    }
    const auto targetId = world.addActor(makePlayer(targetFeet));
    const auto npcId = addWalkingNpc(world);
    actor(world, npcId).pounce = advanced_platformer::Pounce{{.range = 40.0F}, phase};
    brain(world, npcId).target = targetId;
    tests::perception(world, npcId).targetVisible = visible;

    const advanced_platformer::NpcFacts facts = factsOf(map, world, npcId);

    REQUIRE(facts.targetInPounceRange == inRange);
    REQUIRE(facts.pounceReady == (phase == advanced_platformer::PouncePhase::Ready));
    REQUIRE(facts.pouncing == (phase == advanced_platformer::PouncePhase::Airborne));
}

TEST_CASE("Heard landings and blocked walking are facts", "[npc][facts]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"............", "............", "############"});
    advanced_platformer::World world;
    const auto npcId = addWalkingNpc(world);
    REQUIRE_FALSE(factsOf(map, world, npcId).heardLanding);
    REQUIRE_FALSE(factsOf(map, world, npcId).movementBlocked);

    tests::perception(world, npcId).heardLanding = true;
    tests::platformerMovement(actor(world, npcId)).blocked = true;
    REQUIRE(factsOf(map, world, npcId).heardLanding);
    REQUIRE(factsOf(map, world, npcId).movementBlocked);
}
