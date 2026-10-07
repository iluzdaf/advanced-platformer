#pragma once

#include <optional>

namespace advanced_platformer
{
    struct WorldEvent;

    struct CameraShakeEffect
    {
        float duration = 0.0F;
        float magnitude = 0.0F;
    };

    struct PresentationEffects
    {
        std::optional<CameraShakeEffect> shake;
    };

    class PresentationScripts
    {
    public:
        virtual ~PresentationScripts() = default;

        virtual PresentationEffects onEvent(const WorldEvent& event, bool player) = 0;
    };
}
