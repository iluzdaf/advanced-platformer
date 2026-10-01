#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/movement/surface_climb.hpp"
#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/navigation/navigation_fill.hpp"
#include "advanced_platformer/navigation/route.hpp"
#include "advanced_platformer/navigation/navigation_path.hpp"
#include "advanced_platformer/navigation/platformer_cells.hpp"
#include "advanced_platformer/navigation/platformer_connections.hpp"
#include "advanced_platformer/navigation/actor_navigation.hpp"
#include "advanced_platformer/navigation/platformer_traversal_profile.hpp"
#include "advanced_platformer/navigation/traversal.hpp"
#include "advanced_platformer/timing/frame_profile.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/fixed_step.hpp"
#include "support/prepare_navigation_cache.hpp"
#include "support/route_connections.hpp"
#include "support/navigation_paths.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    using advanced_platformer::Cell;
    using advanced_platformer::FrameProfile;
    using advanced_platformer::PlatformerConnectionCache;
    using advanced_platformer::PlatformerTraversalProfile;
    using advanced_platformer::RouteConnection;

    constexpr glm::vec2 BodySize{12.0F, 12.0F};
}

TEST_CASE("A break drops only the cells whose footprint holds it", "[navigation][cache]")
{
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{
        .size = BodySize, .stepSeconds = tests::FixedStepSeconds};
    // Two cells: one swept the tile at (5, 1), the other never came near it.
    cache.storeConnections({0, 1}, profile, {}, {{0, 0}, {6, 2}});
    cache.storeConnections({9, 1}, profile, {}, {{8, 0}, {10, 2}});

    REQUIRE(cache.cachedCellCount(profile) == 2);
    REQUIRE(cache.cellsConnected(profile) == 0);
    cache.storeConnections(
        {9, 1},
        profile,
        {{{{{10, 1}}, advanced_platformer::Traversal::Walk, {}}, 1}},
        {{8, 0}, {10, 2}});
    REQUIRE(cache.cellsConnected(profile) == 1);

    REQUIRE(cache.invalidate({5, 1}) == 1);

    REQUIRE(cache.cachedConnections({0, 1}, profile) == nullptr);
    REQUIRE(cache.cachedConnections({9, 1}, profile) != nullptr);
    REQUIRE(cache.size() == 1);
    REQUIRE(cache.cachedCellCount(profile) == 1);

    cache.storeConnections({0, 1}, profile, {}, {{0, 0}, {6, 2}});
    REQUIRE(cache.cachedCellCount(profile) == 2);
    cache.clear();
    REQUIRE(cache.size() == 0);
}

TEST_CASE("Dropped and queued cells wait in one queue, each once", "[navigation][cache]")
{
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{
        .size = BodySize, .stepSeconds = tests::FixedStepSeconds};
    const PlatformerTraversalProfile other{
        .size = {12.0F, 20.0F}, .stepSeconds = tests::FixedStepSeconds};
    REQUIRE(cache.cellsPending(profile) == 0);
    REQUIRE_FALSE(cache.nextPending(profile).has_value());

    // Three cells swept the tile at (5, 1), for the one profile; one for the other.
    cache.storeConnections({0, 1}, profile, {}, {{0, 0}, {6, 2}});
    cache.storeConnections({1, 1}, profile, {}, {{0, 0}, {6, 2}});
    cache.storeConnections({2, 1}, profile, {}, {{0, 0}, {6, 2}});
    cache.storeConnections({0, 1}, other, {}, {{0, 0}, {6, 2}});
    cache.invalidate({5, 1});
    REQUIRE(cache.cellsPending(profile) == 3);
    REQUIRE(cache.cellsPending(other) == 1);
    REQUIRE(cache.isPending({1, 1}, profile));
    REQUIRE_FALSE(cache.isPending({1, 1}, other));

    // A queued cell joins behind them; one already cached or waiting is not queued again.
    cache.queue({7, 1}, profile);
    cache.queue({7, 1}, profile);
    cache.queue({1, 1}, profile);
    cache.storeConnections({8, 1}, profile, {}, {{8, 0}, {8, 2}});
    cache.queue({8, 1}, profile);
    REQUIRE(cache.cellsPending(profile) == 4);
    REQUIRE(cache.isPending({7, 1}, profile));
    REQUIRE_FALSE(cache.isPending({8, 1}, profile));

    // A cell moved to the front comes next; caching a cell takes it off the queue.
    cache.prioritise({1, 1}, profile);
    REQUIRE(cache.nextPending(profile).value_or(Cell{}) == Cell{1, 1});
    cache.storeConnections({1, 1}, profile, {}, {{0, 0}, {6, 2}});
    REQUIRE(cache.cellsPending(profile) == 3);
    REQUIRE_FALSE(cache.isPending({1, 1}, profile));
    REQUIRE(cache.nextPending(profile).value_or(Cell{1, 1}) != Cell{1, 1});
    // Moving a cell that is not waiting, or one of an unknown profile, changes nothing.
    cache.prioritise({1, 1}, profile);
    cache.prioritise({0, 1}, {.size = {1.0F, 1.0F}, .stepSeconds = tests::FixedStepSeconds});
    REQUIRE(cache.cellsPending(profile) == 3);

    cache.clear();
    REQUIRE(cache.cellsPending(profile) == 0);
}

TEST_CASE("Syncing with the map applies each break once", "[navigation][cache]")
{
    advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........", "###g####"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    PlatformerConnectionCache cache;
    const PlatformerTraversalProfile profile{
        .size = BodySize, .stepSeconds = tests::FixedStepSeconds};
    cache.storeConnections({3, 0}, profile, {}, {{2, 0}, {4, 1}});
    cache.applyRecordedTileBreaks(map);
    REQUIRE(cache.cachedConnections({3, 0}, profile) != nullptr);

    REQUIRE(map.breakTile({3, 1}));
    FrameProfile breaking;
    cache.applyRecordedTileBreaks(map, &breaking);
    REQUIRE(cache.cachedConnections({3, 0}, profile) == nullptr);
    REQUIRE(advanced_platformer::frameStatisticCount(breaking, "Tile breaks applied") == 1);
    REQUIRE(advanced_platformer::frameStatisticCount(breaking, "Cells dropped") == 1);

    // Reapplying the same break log leaves the recached cell intact.
    cache.storeConnections({3, 0}, profile, {}, {{2, 0}, {4, 1}});
    FrameProfile again;
    cache.applyRecordedTileBreaks(map, &again);
    REQUIRE(cache.cachedConnections({3, 0}, profile) != nullptr);
    REQUIRE(advanced_platformer::frameStatisticCount(again, "Tile breaks applied") == 0);
    REQUIRE(advanced_platformer::frameStatisticCount(again, "Cells dropped") == 0);
}

TEST_CASE("A broken wall opens a route once the fill has caught up", "[navigation][cache]")
{
    // A corridor one cell tall with a breakable wall across it: no jump gets over.
    advanced_platformer::TileMap map =
        tests::TileMapBuilder({"########", "#..g...#", "########"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    const Cell start{1, 1};
    const Cell goal{5, 1};
    advanced_platformer::World world;
    world.addActor(
        tests::ActorBuilder::sized(BodySize)
            .atFeet({24.0F, 32.0F})
            .platforming()
            .thinking({64.0F, 1.0F}));
    const PlatformerTraversalProfile profile{
        .size = BodySize, .stepSeconds = tests::FixedStepSeconds};
    tests::prepareNavigationCache(map, world);
    PlatformerConnectionCache& cache = world.platformerConnections();
    const advanced_platformer::Actor platformer =
        tests::ActorBuilder::sized(BodySize).restingAt({start}).platforming();
    const auto search = [&](FrameProfile& frame)
    {
        return advanced_platformer::findActorPath(
                   map,
                   platformer,
                   advanced_platformer::feetInCell(tests::TileSize, goal),
                   profile.stepSeconds,
                   cache,
                   &frame)
            .value();
    };

    FrameProfile blocked;
    const auto blockedResult = search(blocked);
    REQUIRE(blockedResult.status == advanced_platformer::NavigationPathStatus::Unreachable);
    REQUIRE(blockedResult.path.has_value());
    REQUIRE(
        advanced_platformer::endOf(
            blockedResult.path.value_or(advanced_platformer::NavigationPath{})) ==
        advanced_platformer::feetInCell(tests::TileSize, {2, 1}));

    REQUIRE(map.breakTile({3, 1}));

    // The search applies the break, then waits at the dropped start instead of
    // simulating it. The start moves to the front of the fill queue.
    FrameProfile waiting;
    const auto waitingResult = search(waiting);
    REQUIRE(waitingResult.status == advanced_platformer::NavigationPathStatus::Deferred);
    REQUIRE_FALSE(waitingResult.path.has_value());
    REQUIRE(advanced_platformer::frameStatisticCount(waiting, "Paths deferred") == 1);
    REQUIRE(cache.cachedConnections(start, profile) == nullptr);
    REQUIRE(cache.nextPending(profile).value_or(Cell{}) == start);
    const std::size_t pending = cache.cellsPending(profile);
    REQUIRE(pending > 0);

    // A fill with ticks to spare recaches every dropped cell; the search then finds the
    // route through the gap without simulating.
    FrameProfile fillProfile;
    const int cellsCached =
        advanced_platformer::advanceNavigationFill(map, cache, 1000000, &fillProfile);
    REQUIRE(cellsCached == static_cast<int>(pending));
    REQUIRE(advanced_platformer::frameStatisticCount(fillProfile, "Fill simulated ticks") > 0);
    REQUIRE(cache.cellsPending(profile) == 0);
    FrameProfile opened;
    const auto openedResult = search(opened);
    REQUIRE(openedResult.status == advanced_platformer::NavigationPathStatus::Found);
    REQUIRE(openedResult.path.has_value());
    REQUIRE(advanced_platformer::frameStatisticCount(opened, "Paths deferred") == 0);
}

TEST_CASE("A broken floor takes a walk away and gives a fall", "[navigation][cache]")
{
    // An upper floor with a breakable tile, over a lower floor that catches a fall.
    advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........................",
                               "###g####################",
                               "........................",
                               "########################"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    advanced_platformer::World world;
    world.addActor(
        tests::ActorBuilder::sized(BodySize)
            .atFeet({24.0F, 16.0F})
            .platforming()
            .thinking({64.0F, 1.0F}));
    const PlatformerTraversalProfile profile{
        .size = BodySize, .stepSeconds = tests::FixedStepSeconds};
    tests::prepareNavigationCache(map, world);
    PlatformerConnectionCache& cache = world.platformerConnections();
    const auto walksTo = [](const std::vector<RouteConnection>& connections, Cell cell)
    {
        return std::ranges::any_of(
            connections,
            [cell](const RouteConnection& connection)
            {
                return connection.step.traversal == advanced_platformer::Traversal::Walk &&
                       connection.step.destination.cell == cell;
            });
    };
    REQUIRE(walksTo(*cache.cachedConnections({1, 0}, profile), {6, 0}));
    const std::vector<RouteConnection>* farAway = cache.cachedConnections({23, 0}, profile);
    REQUIRE(farAway != nullptr);
    const std::vector<RouteConnection> farAwayBefore = *farAway;
    REQUIRE_FALSE(farAwayBefore.empty());
    const std::size_t cachedBeforeBreak = cache.cachedCellCount(profile);

    REQUIRE(map.breakTile({3, 1}));

    // The walk across the hole is gone, and a fall into it has appeared.
    FrameProfile breaking;
    cache.applyRecordedTileBreaks(map, &breaking);
    const int dropped = advanced_platformer::frameStatisticCount(breaking, "Cells dropped");
    REQUIRE(dropped > 0);
    REQUIRE(cache.cachedCellCount(profile) == cachedBeforeBreak - dropped);
    auto firstBuild = advanced_platformer::buildPlatformerConnections(map, {1, 0}, profile, &cache);
    advanced_platformer::storePlatformerConnections(cache, {1, 0}, profile, std::move(firstBuild));
    const std::vector<RouteConnection>* afterBreak = cache.cachedConnections({1, 0}, profile);
    REQUIRE(afterBreak != nullptr);
    REQUIRE_FALSE(walksTo(*afterBreak, {6, 0}));
    auto edgeBuild = advanced_platformer::buildPlatformerConnections(map, {2, 0}, profile, &cache);
    advanced_platformer::storePlatformerConnections(cache, {2, 0}, profile, std::move(edgeBuild));
    const std::vector<RouteConnection>* fromTheEdge = cache.cachedConnections({2, 0}, profile);
    REQUIRE(fromTheEdge != nullptr);
    REQUIRE(
        std::ranges::any_of(
            *fromTheEdge,
            [](const RouteConnection& connection)
            {
                return connection.step.traversal == advanced_platformer::Traversal::Fall &&
                       connection.step.destination.cell == Cell{3, 2};
            }));
    // A cell whose simulations never came near the hole was left as it was.
    const std::vector<RouteConnection>* farAwayAfter = cache.cachedConnections({23, 0}, profile);
    REQUIRE(farAwayAfter != nullptr);
    tests::requireSameRouteConnections(*farAwayAfter, farAwayBefore);
    REQUIRE(cache.cachedCellCount(profile) == cachedBeforeBreak - dropped + 2);
}

TEST_CASE("A broken climbable tile takes its climbs away", "[navigation][cache][climb]")
{
    using advanced_platformer::ClimbSurface;
    using advanced_platformer::RouteLocation;
    advanced_platformer::TileMap map =
        tests::TileMapBuilder({"......", ".c....", ".g....", ".c....", "######"})
            .where('c', tests::Tile().blocksMovement().climbable())
            .where('g', tests::Tile().blocksMovement().climbable().breaksInto('.'));
    const advanced_platformer::SurfaceClimbConfig climbing{60.0F};
    const PlatformerTraversalProfile climber{
        .size = BodySize, .stepSeconds = tests::FixedStepSeconds, .climb = climbing};
    const RouteLocation start{{2, 3}};
    const RouteLocation onWall{{2, 1}, ClimbSurface::LeftWall};
    const glm::vec2 wallFeet = advanced_platformer::feetOf(
        advanced_platformer::boundsAtSurface(tests::TileSize, onWall, BodySize));
    PlatformerConnectionCache cache;
    tests::fillConnections(map, cache, climber);
    const advanced_platformer::Actor climberActor =
        tests::ActorBuilder::sized(BodySize).restingAt(start).platforming().climbing(climbing);
    const auto search = [&]()
    {
        return advanced_platformer::findActorPath(
                   map, climberActor, wallFeet, climber.stepSeconds, cache)
            .value();
    };

    const auto climbed = search();
    REQUIRE(climbed.status == advanced_platformer::NavigationPathStatus::Found);
    REQUIRE(
        advanced_platformer::endOf(climbed.path.value_or(advanced_platformer::NavigationPath{})) ==
        wallFeet);

    REQUIRE(map.breakTile({1, 2}));

    // The cells that climbed past the tile wait for the fill; then the wall above
    // the gap is out of reach, and the path stops in the cell below it.
    REQUIRE(search().status == advanced_platformer::NavigationPathStatus::Deferred);
    advanced_platformer::advanceNavigationFill(map, cache, 1000000);
    const auto stopped = search();
    REQUIRE(stopped.status == advanced_platformer::NavigationPathStatus::Unreachable);
    REQUIRE(
        advanced_platformer::cellAtFeet(
            tests::TileSize,
            advanced_platformer::endOf(
                stopped.path.value_or(advanced_platformer::NavigationPath{}))) == Cell{2, 3});
}
