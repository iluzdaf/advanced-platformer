#include "game_catalogs.hpp"

#include "actor_catalog.hpp"
#include "animation_catalog.hpp"
#include "camera_settings.hpp"
#include "exit_catalog.hpp"
#include "hud_catalog.hpp"
#include "item_catalog.hpp"
#include "machine_catalog.hpp"
#include "pickup_catalog.hpp"
#include "room_pieces.hpp"
#include "tile_catalog.hpp"
#include "sound_catalog.hpp"

#include <filesystem>

namespace advanced_platformer
{
    GameCatalogs loadGameCatalogs(const std::filesystem::path& catalogDirectory)
    {
        GameCatalogs catalogs;
        catalogs.tiles = loadTileCatalog(catalogDirectory / "tiles.json");
        catalogs.animations = loadAnimationCatalog(catalogDirectory / "animations.json");
        catalogs.machines = loadMachineCatalog(catalogDirectory / "machines.json");
        catalogs.actors = loadActorCatalog(
            catalogDirectory / "actors.json", catalogs.animations, catalogs.machines);
        catalogs.items = loadItemCatalog(catalogDirectory / "items.json");
        catalogs.pickups = loadPickupCatalog(catalogDirectory / "pickups.json", catalogs.items);
        catalogs.exits = loadExitCatalog(catalogDirectory / "exits.json");
        catalogs.hudIcons = loadHudIcons(catalogDirectory / "hud.json");
        catalogs.sounds = loadSoundCatalog(catalogDirectory / "sounds.json");
        catalogs.camera = loadCameraSettings(catalogDirectory / "camera.json");
        catalogs.pieces = loadRoomPieceCatalog(catalogDirectory / "pieces.json");
        return catalogs;
    }
}
