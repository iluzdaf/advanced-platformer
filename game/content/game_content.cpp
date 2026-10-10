#include "game_content.hpp"

#include "game_catalogs.hpp"
#include "npc_script_catalog.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace advanced_platformer
{
    GameContent loadGameContent(const std::filesystem::path& assetDirectory)
    {
        GameContent content;
        content.gameCatalogs = loadGameCatalogs(assetDirectory / "catalogs");
        loadNpcActivityScripts(
            content.npcScripts, content.gameCatalogs.machines, assetDirectory / "scripts");
        std::vector<std::string> soundNames;
        for (const auto& [name, sound] : content.gameCatalogs.sounds)
        {
            soundNames.push_back(name);
        }
        content.presentation.setSoundNames(soundNames);
        content.presentation.loadScript(assetDirectory / "scripts" / "presentation.lua");
        return content;
    }
}
