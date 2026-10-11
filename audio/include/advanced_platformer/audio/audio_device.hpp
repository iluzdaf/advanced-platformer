#pragma once

#include <memory>

#include "advanced_platformer/audio/sound_patch.hpp"

namespace advanced_platformer
{
    class AudioDevice
    {
    public:
        AudioDevice();
        ~AudioDevice();
        AudioDevice(const AudioDevice&) = delete;
        AudioDevice& operator=(const AudioDevice&) = delete;

        bool play(std::shared_ptr<const SoundBuffer> sound, bool loop = false);
        bool replace(std::shared_ptr<const SoundBuffer> sound, bool loop = false);
        bool stop();
        void collectFinished();

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation;
    };
}
