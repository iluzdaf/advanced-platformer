#pragma once

#include <cstdint>
#include <vector>

namespace advanced_platformer
{
    struct SoundPatch
    {
        int waveType = 0;
        double attack = 0;
        double sustain = 0.3;
        double punch = 0;
        double decay = 0.4;
        double baseFrequency = 0.3;
        double frequencyLimit = 0;
        double frequencySlide = 0;
        double deltaSlide = 0;
        double vibratoDepth = 0;
        double vibratoSpeed = 0;
        double changeAmount = 0;
        double changeSpeed = 0;
        double duty = 0;
        double dutySweep = 0;
        double repeatSpeed = 0;
        double phaserOffset = 0;
        double phaserSweep = 0;
        double lowPassCutoff = 1;
        double lowPassSweep = 0;
        double lowPassResonance = 0;
        double highPassCutoff = 0;
        double highPassSweep = 0;
        double volume = 0.5;
        int sampleRate = 44100;
        int sampleSize = 8;
        bool oldParams = true;
    };

    struct SoundBuffer
    {
        std::vector<float> samples;
        int sampleRate = 44100;
    };

    void validateSoundPatch(const SoundPatch& patch);
    SoundBuffer renderSound(const SoundPatch& patch, std::uint32_t seed = 1);
}
