#pragma once

#include <cstdint>
#include <random>

#include <glm/vec2.hpp>

#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/math/coordinates.hpp"

namespace advanced_platformer
{
    class TileMap;

    struct Camera
    {
        glm::vec2 position = {0.0F, 0.0F};
        glm::vec2 viewportSize = InternalViewportSize;
    };

    struct CameraController
    {
        CameraController(Camera initialCamera, glm::vec2 initialDeadZoneSize);

        Camera camera;
        glm::vec2 deadZoneSize;
    };

    Camera makeLockedCamera(
        const TileMap& map,
        const Aabb& target,
        glm::vec2 viewportSize = InternalViewportSize);

    CameraController makeCameraController(
        const TileMap& map,
        const Aabb& target,
        glm::vec2 deadZoneSize,
        glm::vec2 viewportSize = InternalViewportSize);

    void followTarget(CameraController& controller, const TileMap& map, const Aabb& target);

    class CameraShake
    {
    public:
        explicit CameraShake(std::uint32_t seed = 0);

        void start(float duration, float magnitude);
        void update(float deltaTime);
        glm::vec2 offset() const;
        bool active() const;

    private:
        std::mt19937 generator;
        std::uniform_real_distribution<float> unit{-1.0F, 1.0F};
        float duration = 0.0F;
        float magnitude = 0.0F;
        float elapsed = 0.0F;
        bool running = false;
        glm::vec2 current = {0.0F, 0.0F};
    };

    glm::vec2 worldToScreen(const Camera& camera, glm::vec2 worldPosition);
    glm::vec2 screenToWorld(const Camera& camera, glm::vec2 screenPosition);
}
