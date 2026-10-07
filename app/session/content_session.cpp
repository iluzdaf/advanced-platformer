#include "session/content_session.hpp"

#include <exception>
#include <filesystem>

#include <glm/vec2.hpp>
#include <format>
#include <utility>

#include "session/atlas_regions.hpp"
#include "content/game_content.hpp"
#include "diagnostics/console_log.hpp"
#include "game.hpp"
#include "level/level_reload.hpp"
#include "graphics/sprite_renderer.hpp"

namespace advanced_platformer
{
    namespace
    {
        constexpr float PollSeconds = 0.25F;

        Image loadAtlas(const std::filesystem::path& assetDirectory)
        {
            return loadImage((assetDirectory / "textures" / "sprites.png").string());
        }

        GameContent loadContentForAtlas(
            const std::filesystem::path& assetDirectory,
            glm::ivec2 atlasSize)
        {
            GameContent content = loadGameContent(assetDirectory);
            validateAtlasRegions(content.gameCatalogs, atlasSize, assetDirectory / "catalogs");
            return content;
        }
    }

    ContentSession::ContentSession(
        std::filesystem::path directory,
        SpriteRenderer& spriteRenderer,
        bool watchAssets)
        : assetDirectory(std::move(directory)),
          renderer(spriteRenderer),
          textureId(spriteRenderer.loadTexture(loadAtlas(assetDirectory)))
    {
        if (watchAssets)
        {
            watcher.emplace(assetDirectory);
        }
    }

    GameContent ContentSession::load() const
    {
        const Texture texture = atlas();
        return loadContentForAtlas(assetDirectory, {texture.width, texture.height});
    }

    bool ContentSession::assetsChanged()
    {
        if (!watcher.has_value() || pollClock.elapsedSeconds() < PollSeconds)
        {
            return false;
        }

        pollClock.lapSeconds();
        return watcher->poll();
    }

    void ContentSession::reload(Game& game, ConsoleLog& console)
    {
        try
        {
            const Image image = loadAtlas(assetDirectory);
            const LevelReload reload =
                game.reload(loadContentForAtlas(assetDirectory, {image.width, image.height}));
            renderer.replaceTexture(textureId, image);
            console.write(ConsoleLevel::Info, describeReload(reload));
        }
        catch (const std::exception& error)
        {
            console.write(
                ConsoleLevel::Error, std::format("Could not reload content: {}", error.what()));
        }
    }

    int ContentSession::atlasTextureId() const
    {
        return textureId;
    }

    Texture ContentSession::atlas() const
    {
        return renderer.texture(textureId);
    }
}
