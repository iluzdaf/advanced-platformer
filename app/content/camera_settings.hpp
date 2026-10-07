#pragma once

#include <filesystem>
#include <string_view>

#include <glm/vec2.hpp>

namespace advanced_platformer
{
    struct CameraSettings
    {
        glm::vec2 deadZone = {0.0F, 0.0F};
    };

    CameraSettings parseCameraSettings(std::string_view text, std::string_view sourceName);
    CameraSettings loadCameraSettings(const std::filesystem::path& path);
}
