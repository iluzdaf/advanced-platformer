#include "camera_settings.hpp"

#include "content_diagnostics.hpp"
#include "content_glaze.hpp"

#include <filesystem>
#include <format>
#include <string_view>

#include <glaze/glaze.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/math/coordinates.hpp"
#include "advanced_platformer/math/validation.hpp"

namespace advanced_platformer
{
    struct CameraSettingsJson
    {
        glm::vec2 deadZone{};
    };

    CameraSettings parseCameraSettings(std::string_view text, std::string_view sourceName)
    {
        const auto file = readContent<CameraSettingsJson>(text, sourceName);
        if (!isFinitePositive(file.deadZone) || file.deadZone.x > InternalViewportSize.x ||
            file.deadZone.y > InternalViewportSize.y)
        {
            failJson(
                sourceName,
                "deadZone",
                std::format(
                    "expected a positive size that fits in the {} by {} view",
                    InternalWidth,
                    InternalHeight));
        }
        return {file.deadZone};
    }

    CameraSettings loadCameraSettings(const std::filesystem::path& path)
    {
        return parseCameraSettings(loadContentText(path), path.string());
    }
}
