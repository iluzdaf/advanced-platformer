#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "debug/navigation_debug.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/navigation/navigation_fill.hpp"
#include "advanced_platformer/navigation/traversal.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/prepare_navigation_cache.hpp"
#include "support/fixed_step.hpp"
#include "support/tile_map_builder.hpp"

TEST_CASE(
    "Navigation debug data shows what the connection cache keeps per cell",
    "[app][debug][navigation]")
{
    advanced_platformer::TileMap map =
        tests::TileMapBuilder({".....", ".....", "##g##"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    advanced_platformer::World world;
    // Without a platformer NPC there is nothing to show.
    REQUIRE_FALSE(
        advanced_platformer::makeNavigationCacheDebugInfo(world, map, tests::FixedStepSeconds)
            .has_value());

    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .atFeet({8.0F, 32.0F})
                       .platforming()
                       .thinking({64.0F, 1.0F}));
    const auto cellsOf = [&]
    {
        return advanced_platformer::makeNavigationCacheDebugInfo(
                   world, map, tests::FixedStepSeconds)
            .value_or(advanced_platformer::NavigationCacheDebugInfo{})
            .cells;
    };
    // Every standable cell is listed; before caching, none has a connection count.
    std::vector<advanced_platformer::NavigationCellDebugInfo> cells = cellsOf();
    REQUIRE(cells.size() == 5);
    REQUIRE(cells.front().bounds.topLeft == glm::vec2{0.0F, 16.0F});
    REQUIRE(cells.front().bounds.size == glm::vec2{16.0F, 16.0F});
    REQUIRE(std::all_of(
        cells.begin(),
        cells.end(),
        [](const advanced_platformer::NavigationCellDebugInfo& cell)
        { return !cell.connections.has_value(); }));

    // Filled, every cell has its connections counted.
    tests::prepareNavigationCache(map, world);
    cells = cellsOf();
    REQUIRE(std::all_of(
        cells.begin(),
        cells.end(),
        [](const advanced_platformer::NavigationCellDebugInfo& cell)
        { return cell.connections.has_value() && *cell.connections > 0; }));

    // A break the cache has synced with leaves the cells it dropped missing.
    REQUIRE(map.breakTile({2, 2}));
    world.platformerConnections().applyRecordedTileBreaks(map);
    cells = cellsOf();
    // The cell over the hole can no longer be stood on, so it is not listed; the hole
    // itself can, since the map's floor blocks beneath it, and it was never cached.
    REQUIRE(cells.size() == 5);
    const auto listed = [&cells](glm::vec2 position)
    {
        return std::any_of(
            cells.begin(),
            cells.end(),
            [position](const advanced_platformer::NavigationCellDebugInfo& cell)
            { return cell.bounds.topLeft == position; });
    };
    REQUIRE_FALSE(listed({32.0F, 16.0F}));
    REQUIRE(listed({32.0F, 32.0F}));
    REQUIRE(std::none_of(
        cells.begin(),
        cells.end(),
        [](const advanced_platformer::NavigationCellDebugInfo& cell)
        { return cell.connections.has_value(); }));
}

TEST_CASE(
    "Navigation debug data shows the selected traversal profile and cache totals",
    "[app][debug][navigation]")
{
    advanced_platformer::TileMap map =
        tests::TileMapBuilder({".....", ".....", "##g##"})
            .where('g', tests::Tile().blocksMovement().breaksInto('.'));
    advanced_platformer::World world;
    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .atFeet({8.0F, 32.0F})
                       .platforming()
                       .thinking({64.0F, 1.0F}));
    world.addActor(tests::ActorBuilder::sized({12.0F, 20.0F})
                       .atFeet({40.0F, 32.0F})
                       .platforming()
                       .thinking({64.0F, 1.0F}));
    const auto infoFor = [&](std::size_t profileIndex)
    {
        advanced_platformer::NavigationDebugView view;
        view.profileIndex = profileIndex;
        view.namedProfiles = {
            {"soldier", {{12.0F, 20.0F}, {}, tests::FixedStepSeconds}},
            {"zombie", {{12.0F, 12.0F}, {}, tests::FixedStepSeconds}}};
        return advanced_platformer::makeNavigationCacheDebugInfo(
                   world, map, tests::FixedStepSeconds, view)
            .value_or(advanced_platformer::NavigationCacheDebugInfo{});
    };

    // The index picks a profile in the order first found and wraps; an actor name is
    // shown when the matching profile has one.
    REQUIRE(advanced_platformer::makeNavigationCacheDebugInfo(world, map, tests::FixedStepSeconds)
                .value_or(advanced_platformer::NavigationCacheDebugInfo{})
                .actorName.empty());
    REQUIRE(infoFor(0).profileCount == 2);
    REQUIRE(infoFor(0).profileIndex == 0);
    REQUIRE(infoFor(0).bodySize == glm::vec2{12.0F, 12.0F});
    REQUIRE(infoFor(0).actorName == "zombie");
    REQUIRE(infoFor(1).profileIndex == 1);
    REQUIRE(infoFor(1).bodySize == glm::vec2{12.0F, 20.0F});
    REQUIRE(infoFor(1).actorName == "soldier");
    REQUIRE(infoFor(2).profileIndex == 0);

    // The totals follow the cache through a fill and a break.
    REQUIRE(infoFor(0).cachedCellCount == 0);
    tests::prepareNavigationCache(map, world);
    const advanced_platformer::NavigationCacheDebugInfo filled = infoFor(0);
    REQUIRE(filled.cachedCellCount == 15);
    REQUIRE(filled.cellsConnected == 5);
    REQUIRE(filled.cachedWalkCount > 0);

    REQUIRE(map.breakTile({2, 2}));
    world.platformerConnections().applyRecordedTileBreaks(map);
    const advanced_platformer::NavigationCacheDebugInfo broken = infoFor(0);
    REQUIRE(broken.cachedCellCount < 15);
    REQUIRE(broken.cellsPending == 15 - broken.cachedCellCount);
    for (std::size_t step = 0; step < broken.cellsPending && infoFor(0).cellsPending > 0; ++step)
    {
        advanced_platformer::advanceNavigationFill(
            map, world.platformerConnections(), advanced_platformer::NavigationFillTicksPerStep);
    }
    REQUIRE(infoFor(0).cellsPending == 0);
}

TEST_CASE(
    "Navigation debug data shows the cache's entry for the cursor cell",
    "[app][debug][navigation]")
{
    // A floor with a step up at the right, so the cursor cell has walks and jumps.
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"........", "......##", "########"});
    advanced_platformer::World world;
    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .atFeet({8.0F, 32.0F})
                       .platforming()
                       .thinking({64.0F, 1.0F}));
    const auto infoAt = [&](std::optional<glm::vec2> cursor)
    {
        advanced_platformer::NavigationDebugView view;
        view.cursorWorld = cursor;
        return advanced_platformer::makeNavigationCacheDebugInfo(
                   world, map, tests::FixedStepSeconds, view)
            .value_or(advanced_platformer::NavigationCacheDebugInfo{});
    };

    // No cursor, or one off the map, shows no cell; an uncached cell shows its bounds only.
    REQUIRE_FALSE(infoAt(std::nullopt).cursorCell.has_value());
    REQUIRE_FALSE(infoAt(glm::vec2{-1.0F, 20.0F}).cursorCell.has_value());
    const advanced_platformer::CursorCellDebugInfo uncached =
        infoAt(glm::vec2{40.0F, 20.0F})
            .cursorCell.value_or(advanced_platformer::CursorCellDebugInfo{});
    REQUIRE(uncached.bounds.topLeft == glm::vec2{32.0F, 16.0F});
    REQUIRE_FALSE(uncached.footprint.has_value());
    REQUIRE(uncached.connections.empty());

    // Once cached, the cell shows its footprint and connections, jumps along their arcs.
    tests::prepareNavigationCache(map, world);
    const advanced_platformer::CursorCellDebugInfo cached =
        infoAt(glm::vec2{40.0F, 20.0F})
            .cursorCell.value_or(advanced_platformer::CursorCellDebugInfo{});
    REQUIRE(cached.footprint.has_value());
    const advanced_platformer::Aabb footprint =
        cached.footprint.value_or(advanced_platformer::Aabb{});
    REQUIRE(footprint.topLeft.x <= 32.0F);
    REQUIRE(advanced_platformer::rightOf(footprint) >= 48.0F);
    REQUIRE_FALSE(cached.connections.empty());
    bool sawWalk = false;
    bool sawArc = false;
    for (const advanced_platformer::CachedConnectionDebugInfo& connection : cached.connections)
    {
        REQUIRE(connection.fromFeet == glm::vec2{40.0F, 32.0F});
        REQUIRE(connection.cost > 0);
        if (connection.traversal == advanced_platformer::Traversal::Walk)
        {
            sawWalk = true;
            REQUIRE(connection.sampledFeet.empty());
        }
        else
        {
            sawArc = true;
            REQUIRE(connection.sampledFeet.size() > 1);
            REQUIRE(connection.sampledFeet.front() == connection.fromFeet);
        }
    }
    REQUIRE(sawWalk);
    REQUIRE(sawArc);
}

TEST_CASE(
    "Navigation debug data lists the cells a climber can only hold a wall or ceiling in",
    "[app][debug][navigation][climb]")
{
    // A room walled and roofed with climbable tiles.
    const advanced_platformer::TileMap map =
        tests::TileMapBuilder({"cccccc", "c.....", "c.....", "c.....", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    const auto infoFor = [&map](advanced_platformer::World& world)
    {
        return advanced_platformer::makeNavigationCacheDebugInfo(
                   world, map, tests::FixedStepSeconds)
            .value_or(advanced_platformer::NavigationCacheDebugInfo{});
    };
    const auto isListed =
        [](const advanced_platformer::NavigationCacheDebugInfo& info, glm::vec2 position)
    {
        return std::any_of(
            info.cells.begin(),
            info.cells.end(),
            [position](const advanced_platformer::NavigationCellDebugInfo& cell)
            { return cell.bounds.topLeft == position; });
    };

    // A walker is shown only the floor.
    advanced_platformer::World walkers;
    walkers.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                         .inCell({3, 3})
                         .platforming()
                         .thinking({64.0F, 1.0F}));
    const advanced_platformer::NavigationCacheDebugInfo walking = infoFor(walkers);
    REQUIRE(walking.cells.size() == 5);
    REQUIRE_FALSE(isListed(walking, {16.0F, 16.0F}));

    // A climber is also shown the cells along the wall and under the ceiling, marked as
    // ones it cannot stand in, and they are counted once cached.
    advanced_platformer::World climbers;
    climbers.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                          .inCell({3, 3})
                          .platforming()
                          .climbing({60.0F})
                          .thinking({64.0F, 1.0F}));
    tests::prepareNavigationCache(map, climbers);
    const advanced_platformer::NavigationCacheDebugInfo climbing = infoFor(climbers);
    REQUIRE(climbing.cells.size() > walking.cells.size());
    for (const advanced_platformer::NavigationCellDebugInfo& cell : climbing.cells)
    {
        // The floor row stands; the rows above are held on the wall or ceiling.
        REQUIRE(cell.standable == (cell.bounds.topLeft.y == 48.0F));
        REQUIRE(cell.connections.has_value());
        REQUIRE(cell.connections.value_or(0) > 0);
    }
    REQUIRE(isListed(climbing, {16.0F, 16.0F}));
    REQUIRE(isListed(climbing, {16.0F, 32.0F}));
    REQUIRE(isListed(climbing, {48.0F, 16.0F}));
}
