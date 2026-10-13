#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "advanced_platformer/audio/music.hpp"
#include "advanced_platformer/audio/sound_mixer.hpp"
#include "advanced_platformer/audio/sound_patch.hpp"

namespace advanced_platformer
{
    class MusicPlayback
    {
    public:
        MusicPlayback(
            MusicParts parts,
            std::string_view backing,
            double beatsPerBar = 4,
            double fadeSeconds = 0.05);
        bool setPartActive(std::string_view track, bool enabled);
        void mix(std::span<float> output) noexcept;
        void stop() noexcept;

    private:
        struct Part
        {
            const SoundBuffer* sound = nullptr;
            float gain = 0;
            float target = 0;
            float step = 0;
            std::size_t remaining = 0;
            std::uint64_t boundary = 0;
            bool pending = false;
            bool enabled = false;
        };

        struct Command
        {
            std::size_t part = 0;
            bool enabled = false;
        };

        void consumeCommands() noexcept;
        float nextSample() noexcept;

        MusicParts music;
        std::array<Part, SoundVoiceCount> parts{};
        std::array<std::string, SoundVoiceCount> names{};
        std::size_t partCount = 0;
        std::size_t backingIndex = 0;
        std::size_t loopFrames = 0;
        std::size_t fadeFrames = 0;
        double framesPerBar = 0;
        std::uint64_t position = 0;
        bool running = true;
        static constexpr std::size_t QueueSize = 65;
        std::array<Command, QueueSize> commands{};
        std::atomic<std::size_t> readIndex{0};
        std::atomic<std::size_t> writeIndex{0};
    };
}
