#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <string_view>
#include <optional>
#include <span>

#include "advanced_platformer/audio/sound_mixer.hpp"
#include "advanced_platformer/audio/sound_patch.hpp"

namespace advanced_platformer
{
    class MusicPlayback;

    class AudioPlayback
    {
    public:
        AudioPlayback();
        explicit AudioPlayback(std::unique_ptr<MusicPlayback> music);
        ~AudioPlayback();
        bool play(std::shared_ptr<const SoundBuffer> sound, bool loop = false);
        bool replace(std::shared_ptr<const SoundBuffer> sound, bool loop = false);
        bool setMusic(std::shared_ptr<const SoundBuffer> sound);
        bool setPartActive(std::string_view track, bool enabled);
        bool stop();
        void collectFinished();
        void render(std::span<float> output) noexcept;

    private:
        enum class CommandKind
        {
            Play,
            Replace,
            Music,
            Stop
        };

        struct Command
        {
            const SoundBuffer* sound = nullptr;
            std::size_t slot = 0;
            bool loop = false;
            CommandKind kind = CommandKind::Stop;
        };

        bool enqueue(std::shared_ptr<const SoundBuffer> sound, bool loop, CommandKind kind);

        static constexpr std::size_t QueueSize = SoundVoiceCount + 1;
        std::array<Command, QueueSize> commands{};
        std::atomic<std::size_t> readIndex{0};
        std::atomic<std::size_t> writeIndex{0};
        std::array<std::shared_ptr<const SoundBuffer>, SoundVoiceCount> retained;
        std::array<std::atomic<bool>, SoundVoiceCount> finished;
        std::array<bool, SoundVoiceCount> active{};
        SoundMixer mixer;
        std::unique_ptr<MusicPlayback> musicParts;
        std::optional<std::size_t> musicSlot;
    };
}
