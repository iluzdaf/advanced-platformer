#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/navigation/route.hpp"
#include "advanced_platformer/navigation/platformer_connections.hpp"
#include "advanced_platformer/navigation/platformer_traversal_profile.hpp"
#include "advanced_platformer/navigation/traversal.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "support/fixed_step.hpp"
#include "support/route_connections.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    using advanced_platformer::Cell;
    using advanced_platformer::CellRange;
    using advanced_platformer::PlatformerConnectionCache;
    using advanced_platformer::PlatformerTraversalProfile;
    using advanced_platformer::RouteConnection;
    using advanced_platformer::WalkSimulationResult;

    constexpr glm::vec2 BodySize{12.0F, 12.0F};
}

TEST_CASE("Caching a cell stores its connections and the walks it simulated", "[navigation][cache]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "###..###", "########"});
    const Cell cell{1, 2};
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{
        .size = BodySize, .stepSeconds = tests::FixedStepSeconds};

    const int simulatedTicks =
        advanced_platformer::cachePlatformerConnections(map, cache, cell, profile);
    REQUIRE(simulatedTicks > 0);
    REQUIRE(cache.cachedWalkCount(profile) > 0);
    const std::vector<RouteConnection>* simulated = cache.cachedConnections(cell, profile);
    REQUIRE(simulated != nullptr);
    REQUIRE_FALSE(simulated->empty());
    REQUIRE(cache.size() == 1);

    REQUIRE(simulated == cache.cachedConnections(cell, profile));
    REQUIRE(cache.size() == 1);
}

TEST_CASE("Connections are cached separately for each profile", "[navigation][cache]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "########"});
    const Cell cell{3, 1};
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{
        .size = BodySize, .stepSeconds = tests::FixedStepSeconds};
    advanced_platformer::cachePlatformerConnections(map, cache, cell, profile);
    REQUIRE(cache.cachedConnections(cell, profile) != nullptr);
    REQUIRE(cache.cachedConnections({0, 1}, profile) == nullptr);

    PlatformerTraversalProfile taller = profile;
    taller.size.y = 20.0F;
    REQUIRE(cache.cachedConnections(cell, taller) == nullptr);
    PlatformerTraversalProfile faster = profile;
    faster.movement.maximumSpeed += 1.0F;
    REQUIRE(cache.cachedConnections(cell, faster) == nullptr);
    PlatformerTraversalProfile finer = profile;
    finer.stepSeconds *= 0.5F;
    REQUIRE(cache.cachedConnections(cell, finer) == nullptr);
    PlatformerTraversalProfile climber = profile;
    climber.climb = advanced_platformer::SurfaceClimbConfig{60.0F};
    REQUIRE(cache.cachedConnections(cell, climber) == nullptr);

    advanced_platformer::cachePlatformerConnections(map, cache, cell, climber);
    PlatformerTraversalProfile sameClimber = profile;
    sameClimber.climb = advanced_platformer::SurfaceClimbConfig{60.0F};
    REQUIRE(cache.cachedConnections(cell, sameClimber) != nullptr);
    PlatformerTraversalProfile quickerClimber = climber;
    quickerClimber.climb->speed += 1.0F;
    REQUIRE(cache.cachedConnections(cell, quickerClimber) == nullptr);

    REQUIRE(advanced_platformer::cachePlatformerConnections(map, cache, cell, taller) > 0);
    REQUIRE(cache.size() == 3);
    REQUIRE(cache.knownProfiles().size() == 3);
    REQUIRE(cache.knownProfiles()[0] == profile);
    REQUIRE(cache.knownProfiles()[1] == climber);
    REQUIRE(cache.knownProfiles()[2] == taller);

    cache.clear();
    REQUIRE(cache.size() == 0);
    REQUIRE(cache.cachedConnections(cell, profile) == nullptr);
}

TEST_CASE("A non-standable cell is cached with no connections", "[navigation][cache]")
{
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "########"});
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{
        .size = BodySize, .stepSeconds = tests::FixedStepSeconds};
    advanced_platformer::cachePlatformerConnections(map, cache, {3, 0}, profile);
    const std::vector<RouteConnection>* connections = cache.cachedConnections({3, 0}, profile);
    REQUIRE(connections != nullptr);
    REQUIRE(connections->empty());
    REQUIRE(cache.size() == 1);
    REQUIRE(cache.cachedConnections({3, 0}, profile) == connections);
}

TEST_CASE(
    "Stored connections are sorted by the surface they leave, in their order within it",
    "[navigation][cache]")
{
    using advanced_platformer::ClimbSurface;
    using advanced_platformer::Traversal;
    const auto leaving = [](ClimbSurface surface, int cost)
    { return RouteConnection{{{{1, 0}}, Traversal::Climb, {}}, cost, surface}; };

    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{
        .size = BodySize, .stepSeconds = tests::FixedStepSeconds};
    cache.storeConnections(
        {0, 0},
        profile,
        {leaving(ClimbSurface::Ceiling, 1),
         leaving(ClimbSurface::None, 2),
         leaving(ClimbSurface::Ceiling, 3),
         leaving(ClimbSurface::None, 4)},
        {{0, 0}, {1, 0}});

    const std::vector<RouteConnection>* stored = cache.cachedConnections({0, 0}, profile);
    REQUIRE(stored != nullptr);
    REQUIRE(stored->size() == 4);
    REQUIRE((*stored)[0].sourceSurface == ClimbSurface::None);
    REQUIRE((*stored)[0].cost == 2);
    REQUIRE((*stored)[1].sourceSurface == ClimbSurface::None);
    REQUIRE((*stored)[1].cost == 4);
    REQUIRE((*stored)[2].sourceSurface == ClimbSurface::Ceiling);
    REQUIRE((*stored)[2].cost == 1);
    REQUIRE((*stored)[3].sourceSurface == ClimbSurface::Ceiling);
    REQUIRE((*stored)[3].cost == 3);
}

TEST_CASE("A walk is cached per length and profile, and a break leaves it", "[navigation][cache]")
{
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{
        .size = BodySize, .stepSeconds = tests::FixedStepSeconds};
    const PlatformerTraversalProfile taller{
        .size = {12.0F, 20.0F}, .stepSeconds = tests::FixedStepSeconds};
    REQUIRE(cache.cachedWalk(3, profile) == nullptr);
    REQUIRE(cache.cachedWalkCount(profile) == 0);

    cache.storeWalk(profile, {3, 35, {{-1, -1}, {4, 1}}, 35});
    cache.storeWalk(profile, {-3, 36, {{-4, -1}, {1, 1}}});
    cache.storeWalk(profile, {12, std::nullopt, {{-1, -1}, {13, 1}}});
    REQUIRE(cache.cachedWalkCount(profile) == 3);
    REQUIRE(cache.cachedWalkCount(taller) == 0);
    REQUIRE(cache.cachedWalk(3, taller) == nullptr);
    const WalkSimulationResult* rightwards = cache.cachedWalk(3, profile);
    REQUIRE(rightwards != nullptr);
    REQUIRE(rightwards->columns == 3);
    REQUIRE(rightwards->cost.value_or(0) == 35);
    REQUIRE(rightwards->sweep.last == Cell{4, 1});
    REQUIRE(rightwards->simulatedTicks == 35);
    REQUIRE(cache.cachedWalk(-3, profile)->cost.value_or(0) == 36);
    REQUIRE_FALSE(cache.cachedWalk(12, profile)->cost.has_value());

    cache.storeWalk(profile, {3, 34, {{-1, -1}, {4, 1}}});
    REQUIRE(cache.cachedWalk(3, profile)->cost.value_or(0) == 34);
    cache.storeConnections({0, 1}, profile, {}, {{-2, 0}, {6, 2}});
    cache.invalidate({2, 1});
    REQUIRE(cache.cachedCellCount(profile) == 0);
    REQUIRE(cache.cachedWalkCount(profile) == 3);

    cache.clear();
    REQUIRE(cache.cachedWalkCount(profile) == 0);
}

TEST_CASE("Cached walks change nothing but the ticks simulated", "[navigation][cache]")
{
    const std::string open(40, '.');
    const std::string floor(40, '#');
    const advanced_platformer::TileMap map = tests::TileMapBuilder({open, open, floor});
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{
        .size = BodySize, .stepSeconds = tests::FixedStepSeconds};
    const Cell first{20, 1};
    const Cell second{25, 1};

    const int firstSimulatedTicks =
        advanced_platformer::cachePlatformerConnections(map, cache, first, profile);
    REQUIRE(firstSimulatedTicks > 0);
    const std::size_t walks = cache.cachedWalkCount(profile);
    REQUIRE(walks > 0);
    bool pastTheLimit = false;
    for (int columns = -map.width(); columns <= map.width(); ++columns)
    {
        const WalkSimulationResult* walk = cache.cachedWalk(columns, profile);
        pastTheLimit = pastTheLimit || (walk != nullptr && !walk->cost.has_value());
    }
    REQUIRE(pastTheLimit);

    const int secondSimulatedTicks =
        advanced_platformer::cachePlatformerConnections(map, cache, second, profile);
    const std::vector<RouteConnection>* cached = cache.cachedConnections(second, profile);
    REQUIRE(cached != nullptr);
    REQUIRE(cache.cachedWalkCount(profile) == walks);
    PlatformerConnectionCache alone;
    const int aloneSimulatedTicks =
        advanced_platformer::cachePlatformerConnections(map, alone, second, profile);
    REQUIRE(secondSimulatedTicks < aloneSimulatedTicks);
    const std::vector<RouteConnection>* simulated = alone.cachedConnections(second, profile);
    REQUIRE(simulated != nullptr);
    tests::requireSameRouteConnections(*cached, *simulated);
    const CellRange cachedFootprint = cache.cachedFootprint(second, profile).value_or(CellRange{});
    const CellRange simulatedFootprint =
        alone.cachedFootprint(second, profile).value_or(CellRange{});
    REQUIRE(cachedFootprint.first == simulatedFootprint.first);
    REQUIRE(cachedFootprint.last == simulatedFootprint.last);
}

TEST_CASE("The cache rejects an invalid profile", "[navigation][cache][validation]")
{
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile flat{
        .size = {12.0F, 0.0F}, .stepSeconds = tests::FixedStepSeconds};
    const PlatformerTraversalProfile stopped{.size = BodySize, .stepSeconds = 0.0F};
    REQUIRE_THROWS_AS(
        cache.storeConnections({0, 0}, flat, {}, {{0, 0}, {0, 0}}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        cache.storeConnections({0, 0}, stopped, {}, {{0, 0}, {0, 0}}), std::invalid_argument);
    REQUIRE_THROWS_AS(cache.storeWalk(flat, {1, 1, {}}), std::invalid_argument);
    REQUIRE_THROWS_AS(cache.queue({0, 0}, stopped), std::invalid_argument);
    REQUIRE(cache.size() == 0);
}
