#include "game_content.hpp"

#include "game_catalogs.hpp"
#include "run_settings.hpp"
#include "npc_script_catalog.hpp"

#include <filesystem>

#include <glm/vec2.hpp>

namespace advanced_platformer
{
    GameContent loadGameContent(const std::filesystem::path& assetDirectory, glm::ivec2 atlasSize)
    {
        GameContent content;
        content.run = loadRunSettings(assetDirectory / "levels" / "run.json");
        content.gameCatalogs = loadGameCatalogs(assetDirectory / "catalogs", atlasSize);
        loadNpcActivityScripts(
            content.npcScripts, content.gameCatalogs.machines, assetDirectory / "scripts");
        content.presentation.loadScript(assetDirectory / "scripts" / "presentation.lua");
        return content;
    }
}
