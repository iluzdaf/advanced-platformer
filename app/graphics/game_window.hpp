#pragma once

#include <functional>
#include <string>

#include <glm/vec2.hpp>

struct GLFWwindow;

namespace advanced_platformer
{
    struct WindowReading
    {
        glm::ivec2 size = {0, 0};
        glm::ivec2 framebufferSize = {0, 0};
        glm::vec2 cursor = {0.0F, 0.0F};
    };

    class GameWindow
    {
    public:
        GameWindow(
            const char* title,
            glm::ivec2 size,
            std::function<void(std::string)> reportError);
        ~GameWindow();

        GameWindow(const GameWindow&) = delete;
        GameWindow& operator=(const GameWindow&) = delete;

        GLFWwindow* handle() const;
        bool shouldClose() const;
        WindowReading read() const;
        void present() const;

    private:
        struct GlfwLibrary
        {
            explicit GlfwLibrary(std::function<void(std::string)> reportError);
            ~GlfwLibrary();
            GlfwLibrary(const GlfwLibrary&) = delete;
            GlfwLibrary& operator=(const GlfwLibrary&) = delete;

            std::function<void(std::string)> reportError;
        };

        GlfwLibrary library;
        GLFWwindow* window = nullptr;
    };
}
