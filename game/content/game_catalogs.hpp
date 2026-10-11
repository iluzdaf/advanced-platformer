#pragma once

#include <filesystem>
#include <memory>

#include "advanced_platformer/audio/sound_patch.hpp"

#include "advanced_platformer/level/room_pieces.hpp"

#include "actor_catalog.hpp"
#include "animation_catalog.hpp"
#include "camera_settings.hpp"
#include "exit_catalog.hpp"
#include "hud_catalog.hpp"
#include "item_catalog.hpp"
#include "machine_catalog.hpp"
#include "pickup_catalog.hpp"
#include "tile_catalog.hpp"
#include "sound_catalog.hpp"

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
        SoundCatalog sounds;
        std::shared_ptr<const SoundBuffer> music;
        RoomPieces pieces;
    };

    GameCatalogs loadGameCatalogs(const std::filesystem::path& catalogDirectory);
}
