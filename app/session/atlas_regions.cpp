#include "session/atlas_regions.hpp"

#include <filesystem>

#include <glm/vec2.hpp>

#include "content/actor_catalog.hpp"
#include "content/animation_catalog.hpp"
#include "content/exit_catalog.hpp"
#include "content/game_catalogs.hpp"
#include "content/hud_catalog.hpp"
#include "content/item_catalog.hpp"
#include "content/pickup_catalog.hpp"
#include "content/tile_catalog.hpp"

namespace advanced_platformer
{
    void validateAtlasRegions(
        const GameCatalogs& catalogs,
        glm::ivec2 atlasSize,
        const std::filesystem::path& catalogDirectory)
    {
        validateTileAtlasRegions(
            catalogs.tiles, atlasSize, (catalogDirectory / "tiles.json").string());
        validateAnimationAtlasRegions(
            catalogs.animations, atlasSize, (catalogDirectory / "animations.json").string());
        validateActorAtlasRegions(
            catalogs.actors, atlasSize, (catalogDirectory / "actors.json").string());
        validateItemAtlasRegions(
            catalogs.items, atlasSize, (catalogDirectory / "items.json").string());
        validatePickupAtlasRegions(
            catalogs.pickups, atlasSize, (catalogDirectory / "pickups.json").string());
        validateExitAtlasRegions(
            catalogs.exits, atlasSize, (catalogDirectory / "exits.json").string());
        validateHudAtlasRegions(
            catalogs.hudIcons, atlasSize, (catalogDirectory / "hud.json").string());
    }
}
