#pragma once

#include <string>
#include <vector>

namespace advanced_platformer
{
    struct RenderScene;

    struct Image
    {
        int width = 0;
        int height = 0;
        std::vector<unsigned char> pixels;
    };

    Image loadImage(const std::string& path);

    struct Texture
    {
        unsigned int handle = 0;
        int width = 0;
        int height = 0;
    };

    class SpriteRenderer
    {
    public:
        SpriteRenderer();
        ~SpriteRenderer();

        SpriteRenderer(const SpriteRenderer&) = delete;
        SpriteRenderer& operator=(const SpriteRenderer&) = delete;

        int loadTexture(const Image& image);
        void replaceTexture(int textureId, const Image& image);
        Texture texture(int textureId) const;
        void render(const RenderScene& scene, int framebufferWidth, int framebufferHeight);

    private:
        unsigned int shader = 0;
        unsigned int vertexArray = 0;
        unsigned int positionBuffer = 0;
        unsigned int textureCoordinateBuffer = 0;
        unsigned int framebuffer = 0;
        unsigned int framebufferTexture = 0;
        int viewportLocation = -1;
        int opacityLocation = -1;
        int whiteFlashLocation = -1;
        int shadeLocation = -1;
        std::vector<Texture> textures;
    };
}
