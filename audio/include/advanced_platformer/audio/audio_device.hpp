#pragma once

#include <memory>
#include <string_view>

#include "advanced_platformer/audio/sound_patch.hpp"

namespace advanced_platformer
{
    class MusicPlayback;

    class AudioDevice
    {
    public:
        AudioDevice();
        explicit AudioDevice(std::unique_ptr<MusicPlayback> music);
        ~AudioDevice();
        AudioDevice(const AudioDevice&) = delete;
        AudioDevice& operator=(const AudioDevice&) = delete;

        bool play(std::shared_ptr<const SoundBuffer> sound, bool loop = false);
        bool replace(std::shared_ptr<const SoundBuffer> sound, bool loop = false);
        bool setMusic(std::shared_ptr<const SoundBuffer> sound);
        bool setPartActive(std::string_view track, bool enabled);
        bool stop();
        void collectFinished();

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation;
    };
}
