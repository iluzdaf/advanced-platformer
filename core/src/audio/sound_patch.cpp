#include "advanced_platformer/audio/sound_patch.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numbers>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace advanced_platformer
{
    namespace
    {
        double square(double value)
        {
            return value * value;
        }

        double cube(double value)
        {
            return value * value * value;
        }
    }

    namespace
    {
        void requireRange(double value, double minimum, std::string_view field)
        {
            if (!std::isfinite(value) || value < minimum || value > 1)
            {
                throw std::invalid_argument(
                    std::format("{} must be finite and in [{}, 1]", field, minimum));
            }
        }
    }

    void validateSoundPatch(const SoundPatch& patch)
    {
        if (patch.waveType < 0 || patch.waveType > 3)
        {
            throw std::invalid_argument(
                "wave_type must be 0 (square), 1 (saw), 2 (sine), or 3 (noise)");
        }
        for (const auto& [field, value] : std::array{
                 std::pair{"p_env_attack", patch.attack},
                 std::pair{"p_env_sustain", patch.sustain},
                 std::pair{"p_env_punch", patch.punch},
                 std::pair{"p_env_decay", patch.decay},
                 std::pair{"p_base_freq", patch.baseFrequency},
                 std::pair{"p_freq_limit", patch.frequencyLimit},
                 std::pair{"p_vib_strength", patch.vibratoDepth},
                 std::pair{"p_vib_speed", patch.vibratoSpeed},
                 std::pair{"p_arp_speed", patch.changeSpeed},
                 std::pair{"p_duty", patch.duty},
                 std::pair{"p_repeat_speed", patch.repeatSpeed},
                 std::pair{"p_lpf_freq", patch.lowPassCutoff},
                 std::pair{"p_lpf_resonance", patch.lowPassResonance},
                 std::pair{"p_hpf_freq", patch.highPassCutoff},
                 std::pair{"sound_vol", patch.volume}})
        {
            requireRange(value, 0, field);
        }
        for (const auto& [field, value] : std::array{
                 std::pair{"p_freq_ramp", patch.frequencySlide},
                 std::pair{"p_freq_dramp", patch.deltaSlide},
                 std::pair{"p_arp_mod", patch.changeAmount},
                 std::pair{"p_duty_ramp", patch.dutySweep},
                 std::pair{"p_pha_offset", patch.phaserOffset},
                 std::pair{"p_pha_ramp", patch.phaserSweep},
                 std::pair{"p_lpf_ramp", patch.lowPassSweep},
                 std::pair{"p_hpf_ramp", patch.highPassSweep}})
        {
            requireRange(value, -1, field);
        }
        if (patch.sampleRate != 44100 && patch.sampleRate != 22050 && patch.sampleRate != 11025)
        {
            throw std::invalid_argument("sample_rate must be 44100, 22050, or 11025");
        }
        if (patch.sampleSize != 8 && patch.sampleSize != 16)
        {
            throw std::invalid_argument("sample_size must be 8 or 16");
        }
        if (!patch.oldParams)
        {
            throw std::invalid_argument("oldParams must be true for jsfxr parameters");
        }
        if (std::floor(square(patch.attack) * 100000) + std::floor(square(patch.sustain) * 100000) +
                std::floor(square(patch.decay) * 100000) <
            1)
        {
            throw std::invalid_argument("the sound envelope must last at least one sample");
        }
    }

    namespace
    {
        struct Oscillator
        {
            double period;
            double periodMultiplier;
            double duty;
            int changeTime;
        };

        Oscillator oscillatorFor(const SoundPatch& patch)
        {
            return {
                100 / (square(patch.baseFrequency) + 0.001),
                1 - cube(patch.frequencySlide) * 0.01,
                0.5 - patch.duty * 0.5,
                patch.changeSpeed == 1
                    ? 0
                    : static_cast<int>(square(1 - patch.changeSpeed) * 20000 + 32)};
        }

        void fillNoise(std::array<double, 32>& noise, std::uint32_t& random)
        {
            for (double& value : noise)
            {
                random = random * 1664525U + 1013904223U;
                value = static_cast<double>(random) / 4294967296.0 * 2 - 1;
            }
        }
    }

    SoundBuffer renderSound(const SoundPatch& patch, std::uint32_t seed)
    {
        validateSoundPatch(patch);
        const std::array envelopeLengths{
            static_cast<int>(square(patch.attack) * 100000),
            static_cast<int>(square(patch.sustain) * 100000),
            static_cast<int>(square(patch.decay) * 100000)};
        SoundBuffer result{{}, patch.sampleRate};
        result.samples.reserve(
            static_cast<std::size_t>(envelopeLengths[0]) +
            static_cast<std::size_t>(envelopeLengths[1]) +
            static_cast<std::size_t>(envelopeLengths[2]) + 3);
        Oscillator oscillator = oscillatorFor(patch);
        const double maximumPeriod = 100 / (square(patch.frequencyLimit) + 0.001);
        const double changeMultiplier = patch.changeAmount >= 0
                                            ? 1 - square(patch.changeAmount) * 0.9
                                            : 1 + square(patch.changeAmount) * 10;
        const int repeatTime = patch.repeatSpeed == 0
                                   ? 0
                                   : static_cast<int>(square(1 - patch.repeatSpeed) * 20000 + 32);
        int repeatElapsed = 0;
        int envelopeStage = 0;
        int envelopeElapsed = 0;
        double vibratoPhase = 0;
        int phase = 0;
        double lowPass = 0;
        double lowPassDelta = 0;
        double highPass = 0;
        double lowPassCutoff = cube(patch.lowPassCutoff) * 0.1;
        const double damping =
            std::min(0.8, 5 / (1 + square(patch.lowPassResonance) * 20) * (0.01 + lowPassCutoff));
        double highPassCutoff = square(patch.highPassCutoff) * 0.1;
        double phaserOffset = std::copysign(square(patch.phaserOffset) * 1020, patch.phaserOffset);
        const double phaserSweep = std::copysign(square(patch.phaserSweep), patch.phaserSweep);
        std::array<double, 1024> phaser{};
        std::size_t phaserPosition = 0;
        std::array<double, 32> noise{};
        fillNoise(noise, seed);
        const int summands = 44100 / patch.sampleRate;
        int summed = 0;
        double sampleSum = 0;
        const double gain = std::exp(patch.volume) - 1;
        for (int tick = 0;; ++tick)
        {
            if (repeatTime != 0 && ++repeatElapsed >= repeatTime)
            {
                oscillator = oscillatorFor(patch);
                repeatElapsed = 0;
            }
            if (oscillator.changeTime != 0 && tick >= oscillator.changeTime)
            {
                oscillator.changeTime = 0;
                oscillator.period *= changeMultiplier;
            }
            oscillator.periodMultiplier -= cube(patch.deltaSlide) * 0.000001;
            oscillator.period *= oscillator.periodMultiplier;
            if (oscillator.period > maximumPeriod)
            {
                oscillator.period = maximumPeriod;
                if (patch.frequencyLimit > 0)
                {
                    break;
                }
            }
            double period = oscillator.period;
            if (patch.vibratoDepth > 0)
            {
                vibratoPhase += square(patch.vibratoSpeed) * 0.01;
                period *= 1 + std::sin(vibratoPhase) * patch.vibratoDepth * 0.5;
            }
            const int integerPeriod = static_cast<int>(std::max(8.0, std::floor(period)));
            oscillator.duty = std::clamp(oscillator.duty - patch.dutySweep * 0.00005, 0.0, 0.5);
            ++envelopeElapsed;
            if (envelopeElapsed > envelopeLengths[static_cast<std::size_t>(envelopeStage)])
            {
                envelopeElapsed = 0;
                ++envelopeStage;
            }
            while (envelopeStage < 3 &&
                   envelopeLengths[static_cast<std::size_t>(envelopeStage)] == 0)
            {
                ++envelopeStage;
            }
            if (envelopeStage == 3)
            {
                break;
            }
            const double envelopeFraction =
                static_cast<double>(envelopeElapsed) /
                envelopeLengths[static_cast<std::size_t>(envelopeStage)];
            const double envelope = envelopeStage == 0 ? envelopeFraction
                                    : envelopeStage == 1
                                        ? 1 + (1 - envelopeFraction) * 2 * patch.punch
                                        : 1 - envelopeFraction;
            phaserOffset += phaserSweep;
            const auto delay =
                static_cast<std::size_t>(std::min(1023.0, std::abs(std::floor(phaserOffset))));
            highPassCutoff =
                std::clamp(highPassCutoff * (1 + patch.highPassSweep * 0.0003), 0.00001, 0.1);
            double sample = 0;
            for (int subSample = 0; subSample < 8; ++subSample)
            {
                if (++phase >= integerPeriod)
                {
                    phase %= integerPeriod;
                    if (patch.waveType == 3)
                    {
                        fillNoise(noise, seed);
                    }
                }
                const double fraction = static_cast<double>(phase) / integerPeriod;
                double value = 0;
                switch (patch.waveType)
                {
                case 0:
                    value = fraction < oscillator.duty ? 0.5 : -0.5;
                    break;
                case 1:
                    value = fraction < oscillator.duty
                                ? -1 + 2 * fraction / oscillator.duty
                                : 1 - 2 * (fraction - oscillator.duty) / (1 - oscillator.duty);
                    break;
                case 2:
                    value = std::sin(fraction * 2 * std::numbers::pi);
                    break;
                case 3:
                    value = noise[static_cast<std::size_t>(phase * 32 / integerPeriod)];
                    break;
                default:
                    std::unreachable();
                }
                const double previousLowPass = lowPass;
                lowPassCutoff =
                    std::clamp(lowPassCutoff * (1 + patch.lowPassSweep * 0.0001), 0.0, 0.1);
                if (patch.lowPassCutoff != 1)
                {
                    lowPassDelta += (value - lowPass) * lowPassCutoff;
                    lowPassDelta -= lowPassDelta * damping;
                }
                else
                {
                    lowPass = value;
                    lowPassDelta = 0;
                }
                lowPass += lowPassDelta;
                highPass += lowPass - previousLowPass;
                highPass -= highPass * highPassCutoff;
                phaser[phaserPosition] = highPass;
                value = highPass + phaser[(phaserPosition + 1024 - delay) & 1023U];
                phaserPosition = (phaserPosition + 1) & 1023U;
                sample += value * envelope;
            }
            sampleSum += sample;
            if (++summed == summands)
            {
                result.samples.push_back(static_cast<float>(sampleSum / summands / 8 * gain));
                sampleSum = 0;
                summed = 0;
            }
        }
        return result;
    }
}
