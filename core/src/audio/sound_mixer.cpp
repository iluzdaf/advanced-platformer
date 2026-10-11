#include "advanced_platformer/audio/sound_mixer.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <span>

#include "advanced_platformer/audio/sound_patch.hpp"

namespace advanced_platformer
{
    void SoundMixer::play(std::size_t slot, const SoundBuffer& sound, bool loop) noexcept
    {
        if (slot < voices.size() && !sound.samples.empty() && sound.sampleRate > 0)
        {
            voices[slot] = {&sound, 0, loop};
        }
    }

    void SoundMixer::stop() noexcept
    {
        voices = {};
    }

    bool SoundMixer::playing(std::size_t slot) const noexcept
    {
        return slot < voices.size() && voices[slot].sound != nullptr;
    }

    void SoundMixer::render(std::span<float> output) noexcept
    {
        std::ranges::fill(output, 0.0F);
        for (Voice& voice : voices)
        {
            if (voice.sound == nullptr)
            {
                continue;
            }
            const SoundBuffer& sound = *voice.sound;
            for (float& sample : output)
            {
                const auto index = static_cast<std::size_t>(voice.position);
                const std::size_t next = voice.loop ? (index + 1) % sound.samples.size()
                                                    : std::min(index + 1, sound.samples.size() - 1);
                const float fraction =
                    static_cast<float>(voice.position - static_cast<double>(index));
                sample +=
                    sound.samples[index] + (sound.samples[next] - sound.samples[index]) * fraction;
                voice.position += static_cast<double>(sound.sampleRate) / SoundOutputRate;
                if (voice.position >= static_cast<double>(sound.samples.size()))
                {
                    if (voice.loop)
                    {
                        voice.position =
                            std::fmod(voice.position, static_cast<double>(sound.samples.size()));
                    }
                    else
                    {
                        voice = {};
                        break;
                    }
                }
            }
        }
        for (float& sample : output)
        {
            sample = std::clamp(sample, -1.0F, 1.0F);
        }
    }
}
