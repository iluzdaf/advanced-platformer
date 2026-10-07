#pragma once

#include <utility>

#include <glm/vec2.hpp>

#include "content/exit_catalog.hpp"
#include "content/game_catalogs.hpp"
#include "content/tile_catalog.hpp"
#include "game/level_composition.hpp"
#include "game/level_generator.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/world/level_validation.hpp"
#include "advanced_platformer/world/tile_map.hpp"
#include "advanced_platformer/world/world.hpp"
#include "support/add_player.hpp"
#include "support/fixed_step.hpp"

namespace tests
{
    inline bool playerReachesExit(
        const advanced_platformer::LevelData& level,
        const advanced_platformer::GameCatalogs& catalogs)
    {
        const advanced_platformer::TileMap map =
            advanced_platformer::composeTileMap(level.mapRows, level.tileLegend, catalogs.tiles);
        advanced_platformer::World world;
        advanced_platformer::Actor player = advanced_platformer::composePlayer(catalogs, 0);
        advanced_platformer::moveFeetTo(
            player.body.bounds, advanced_platformer::feetInCell(map.tileSize(), level.playerSpawn));
        addPlayer(world, std::move(player));
        world.setExit(
            advanced_platformer::composeExit(
                advanced_platformer::exitDefinition(catalogs.exits, level.exit.definitionName),
                0,
                advanced_platformer::feetInCell(map.tileSize(), level.exit.spawn)));
        return advanced_platformer::playerCanReachExit(map, world, FixedStepSeconds);
    }
}
