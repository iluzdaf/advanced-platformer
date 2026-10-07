#pragma once

#include <filesystem>
#include <optional>

#include "content/asset_watcher.hpp"
#include "graphics/sprite_renderer.hpp"
#include "advanced_platformer/timing/stopwatch.hpp"

namespace advanced_platformer
{
    class ConsoleLog;
    class Game;
    struct GameContent;

    class ContentSession
    {
    public:
        ContentSession(
            std::filesystem::path directory,
            SpriteRenderer& spriteRenderer,
            bool watchAssets);

        ContentSession(const ContentSession&) = delete;
        ContentSession& operator=(const ContentSession&) = delete;

        GameContent load() const;
        bool assetsChanged();
        void reload(Game& game, ConsoleLog& console);

        int atlasTextureId() const;
        Texture atlas() const;

    private:
        std::filesystem::path assetDirectory;
        SpriteRenderer& renderer;
        int textureId;
        std::optional<AssetWatcher> watcher;
        Stopwatch pollClock;
    };
}
