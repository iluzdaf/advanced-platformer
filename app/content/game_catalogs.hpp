#pragma once

#include <filesystem>

#include <glm/vec2.hpp>

#include "actor_catalog.hpp"
#include "animation_catalog.hpp"
#include "camera_settings.hpp"
#include "exit_catalog.hpp"
#include "hud_catalog.hpp"
#include "item_catalog.hpp"
#include "machine_catalog.hpp"
#include "pickup_catalog.hpp"
#include "tile_catalog.hpp"

namespace advanced_platformer
{
    struct GameCatalogs
    {
        TileCatalog tiles;
        AnimationCatalog animations;
        MachineCatalog machines;
        ActorCatalog actors;
        ItemCatalog items;
        PickupCatalog pickups;
        ExitCatalog exits;
        HudIcons hudIcons;
        CameraSettings camera;
    };

    GameCatalogs loadGameCatalogs(
        const std::filesystem::path& catalogDirectory,
        glm::ivec2 atlasSize);

    void validateAtlasRegions(
        const GameCatalogs& catalogs,
        glm::ivec2 atlasSize,
        const std::filesystem::path& catalogDirectory);
}
