#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/lifecycle.hpp"
#include "advanced_platformer/combat/touch_damage.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "advanced_platformer/world/world_requests.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using advanced_platformer::Health;

    constexpr advanced_platformer::TouchDamage Spikes{
        .damage = 1,
        .lift = 200.0F,
        .repeatSeconds = 1.0F};

    advanced_platformer::TileMap spikeFloor()
    {
        return tests::TileMapBuilder({".^.", "###"}).where('^', tests::Tile{}.hurts(Spikes));
    }

    advanced_platformer::Actor standingAt(float left)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .at({left, 4.0F})
            .platforming()
            .withHealth(3, 3);
    }

    void step(
        const advanced_platformer::TileMap& map,
        advanced_platformer::World& world,
        float seconds)
    {
        advanced_platformer::WorldRequests requests;
        world.advanceSimulationTime(seconds);
        advanced_platformer::updateTouchDamage(map, world, requests);
        advanced_platformer::updateLifeState(world, requests, seconds);
        advanced_platformer::applyWorldRequests(world, requests);
    }
}

TEST_CASE("A tile that hurts on touch damages and lifts the actor in it", "[combat][touch]")
{
    const advanced_platformer::TileMap map = spikeFloor();
    advanced_platformer::World world;
    const auto inSpikes = world.addActor(standingAt(18.0F));
    const auto beside = world.addActor(standingAt(2.0F));

    step(map, world, 0.1F);

    REQUIRE(tests::component<Health>(world, inSpikes).current == 2);
    REQUIRE(tests::actor(world, inSpikes).body.velocity == glm::vec2{0.0F, -200.0F});
    REQUIRE(tests::component<Health>(world, beside).current == 3);
}

TEST_CASE("Touch damage waits its repeat time after any hurt", "[combat][touch]")
{
    const advanced_platformer::TileMap map = spikeFloor();
    advanced_platformer::World world;
    const auto id = world.addActor(standingAt(18.0F));

    step(map, world, 0.1F);
    step(map, world, 0.5F);
    REQUIRE(tests::component<Health>(world, id).current == 2);

    step(map, world, 0.6F);
    REQUIRE(tests::component<Health>(world, id).current == 1);
}

TEST_CASE("Touch damage spares actors without health or already dying", "[combat][touch]")
{
    const advanced_platformer::TileMap map = spikeFloor();
    advanced_platformer::World world;
    const auto prop =
        world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F}).at({18.0F, 4.0F}).platforming());
    const auto dying = world.addActor(standingAt(18.0F));
    tests::actor(world, dying).life = advanced_platformer::LifeState::Dying;
    tests::actor(world, dying).deathTimeRemaining = 1.0F;

    step(map, world, 0.1F);

    REQUIRE(tests::actor(world, prop).body.velocity == glm::vec2{0.0F, 0.0F});
    REQUIRE(tests::component<Health>(world, dying).current == 3);
}

TEST_CASE("A tile with no lift hurts without knockback", "[combat][touch]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({".^.", "###"})
            .where('^', tests::Tile{}.hurts({.damage = 2, .lift = 0.0F}));
    advanced_platformer::World world;
    const auto id = world.addActor(standingAt(18.0F));

    step(map, world, 0.1F);

    REQUIRE(tests::component<Health>(world, id).current == 1);
    REQUIRE(tests::actor(world, id).body.velocity == glm::vec2{0.0F, 0.0F});
}
