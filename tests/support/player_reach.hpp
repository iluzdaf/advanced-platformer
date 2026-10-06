#pragma once

#include <optional>
#include <variant>

#include <glm/vec2.hpp>

#include "content/game_catalogs.hpp"
#include "content/level_data.hpp"
#include "content/tile_catalog.hpp"
#include "game/level_composition.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/navigation/actor_navigation.hpp"
#include "advanced_platformer/navigation/navigation_path.hpp"
#include "advanced_platformer/navigation/platformer_connection_cache.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "support/fixed_step.hpp"
#include "support/navigation_paths.hpp"

namespace tests
{
    inline glm::vec2 cellFeet(
        const advanced_platformer::TileMap& map,
        const advanced_platformer::LevelPosition& position)
    {
        if (const auto* cell = std::get_if<advanced_platformer::Cell>(&position))
        {
            return advanced_platformer::feetInCell(map.tileSize(), *cell);
        }
        return std::get<glm::vec2>(position);
    }

    inline bool playerReachesExit(
        const advanced_platformer::LevelData& level,
        const advanced_platformer::GameCatalogs& catalogs)
    {
        const advanced_platformer::TileMap map =
            advanced_platformer::composeTileMap(level.mapRows, level.tileLegend, catalogs.tiles);
        advanced_platformer::Actor player = advanced_platformer::composePlayer(catalogs, 0);
        advanced_platformer::moveFeetTo(player.body.bounds, cellFeet(map, level.playerSpawn));
        advanced_platformer::PlatformerConnectionCache cache;
        fillConnections(
            map,
            cache,
            advanced_platformer::platformerTraversalProfileFor(player, FixedStepSeconds));
        const std::optional<advanced_platformer::NavigationPathResult> result =
            advanced_platformer::findActorPath(
                map, player, cellFeet(map, level.exit.spawn), FixedStepSeconds, cache);
        return result.has_value() &&
               result->status == advanced_platformer::NavigationPathStatus::Found;
    }
}
