#include "audio/audio_device.hpp"

#include <format>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>

#include <miniaudio.h>

#include "audio/audio_playback.hpp"
#include "advanced_platformer/audio/sound_mixer.hpp"
#include "advanced_platformer/audio/sound_patch.hpp"

namespace advanced_platformer
{
    struct AudioDevice::Implementation
    {
        AudioPlayback playback;
        // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
        ma_device device{};

        static void render(ma_device* device, void* output, const void*, ma_uint32 frames)
        {
            auto* state = static_cast<Implementation*>(device->pUserData);
            state->playback.render({static_cast<float*>(output), frames});
        }

        Implementation()
        {
            ma_device_config config = ma_device_config_init(ma_device_type_playback);
            config.playback.format = ma_format_f32;
            config.playback.channels = 1;
            config.sampleRate = SoundOutputRate;
            config.dataCallback = render;
            config.pUserData = this;
            const ma_result initialised = ma_device_init(nullptr, &config, &device);
            if (initialised != MA_SUCCESS)
            {
                throw std::runtime_error(
                    std::format(
                        "Cannot initialise audio output: {}", ma_result_description(initialised)));
            }
            const ma_result started = ma_device_start(&device);
            if (started != MA_SUCCESS)
            {
                ma_device_uninit(&device);
                throw std::runtime_error(
                    std::format("Cannot start audio output: {}", ma_result_description(started)));
            }
        }

        ~Implementation()
        {
            ma_device_uninit(&device);
        }
    };

    AudioDevice::AudioDevice()
        : implementation(std::make_unique<Implementation>())
    {
    }

    AudioDevice::~AudioDevice() = default;

    bool AudioDevice::play(std::shared_ptr<const SoundBuffer> sound)
    {
        return implementation->playback.play(std::move(sound));
    }

    void AudioDevice::collectFinished()
    {
        implementation->playback.collectFinished();
    }
}
