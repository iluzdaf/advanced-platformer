#pragma once

#include <optional>
#include <string>

namespace advanced_platformer
{
    struct WorldEvent;

    struct CameraShakeEffect
    {
        float duration = 0.0F;
        float magnitude = 0.0F;
    };

    struct SoundEffect
    {
        std::string name;
    };

    struct PresentationEffects
    {
        std::optional<CameraShakeEffect> shake;
        std::optional<SoundEffect> sound;
    };

    class PresentationScripts
    {
    public:
        virtual ~PresentationScripts() = default;

        virtual PresentationEffects onEvent(const WorldEvent& event, bool player) = 0;
    };
}
