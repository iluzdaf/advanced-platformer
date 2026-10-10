#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "advanced_platformer/audio/sound_patch.hpp"

namespace advanced_platformer
{
    constexpr std::size_t SoundVoiceCount = 32;
    constexpr int SoundOutputRate = 44100;

    class SoundMixer
    {
    public:
        void play(std::size_t slot, const SoundBuffer& sound) noexcept;
        bool playing(std::size_t slot) const noexcept;
        void render(std::span<float> output) noexcept;

    private:
        struct Voice
        {
            const SoundBuffer* sound = nullptr;
            double position = 0;
        };

        std::array<Voice, SoundVoiceCount> voices{};
    };
}
