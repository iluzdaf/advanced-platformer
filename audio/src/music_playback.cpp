#include "advanced_platformer/audio/music_playback.hpp"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "advanced_platformer/audio/music.hpp"
#include "advanced_platformer/audio/sound_mixer.hpp"

namespace advanced_platformer
{
    static_assert(std::atomic<std::size_t>::is_always_lock_free);

    MusicPlayback::MusicPlayback(
        MusicParts prepared,
        std::string_view backing,
        double beatsPerBar,
        double fadeSeconds)
        : music(std::move(prepared))
    {
        if (!std::isfinite(music.tempo) || music.tempo < 20 || music.tempo > 400 ||
            !std::isfinite(beatsPerBar) || beatsPerBar < 1 || beatsPerBar > 16 ||
            !std::isfinite(fadeSeconds) || fadeSeconds < 0 || fadeSeconds > 5 ||
            music.tracks.empty() || music.tracks.size() > SoundVoiceCount ||
            !music.tracks.contains(backing))
        {
            throw std::invalid_argument(
                "music parts need valid tempo, bar length, fade, and backing track");
        }
        loopFrames = music.tracks.begin()->second.samples.size();
        framesPerBar = beatsPerBar * 60 / music.tempo * SoundOutputRate;
        fadeFrames = static_cast<std::size_t>(std::llround(fadeSeconds * SoundOutputRate));
        for (const auto& [name, sound] : music.tracks)
        {
            if (name.empty() || sound.samples.empty() || sound.samples.size() != loopFrames ||
                sound.sampleRate != SoundOutputRate)
            {
                throw std::invalid_argument(
                    "music parts need named, equally sized 44100 Hz buffers");
            }
            for (const float sample : sound.samples)
            {
                if (!std::isfinite(sample))
                {
                    throw std::invalid_argument("music parts contain a non-finite sample");
                }
            }
            names[partCount] = name;
            parts[partCount].sound = &sound;
            if (name == backing)
            {
                backingIndex = partCount;
                parts[partCount].gain = 1;
                parts[partCount].target = 1;
            }
            ++partCount;
        }
    }

    bool MusicPlayback::setPartActive(std::string_view track, bool enabled)
    {
        std::size_t part = 0;
        while (part < partCount && names[part] != track)
        {
            ++part;
        }
        if (part == partCount || part == backingIndex)
        {
            return false;
        }
        const std::size_t write = writeIndex.load(std::memory_order_relaxed);
        const std::size_t next = (write + 1) % QueueSize;
        if (next == readIndex.load(std::memory_order_acquire))
        {
            return false;
        }
        commands[write] = {part, enabled};
        writeIndex.store(next, std::memory_order_release);
        return true;
    }

    void MusicPlayback::consumeCommands() noexcept
    {
        std::size_t read = readIndex.load(std::memory_order_relaxed);
        const std::size_t write = writeIndex.load(std::memory_order_acquire);
        while (read != write)
        {
            const auto command = commands[read];
            auto& part = parts[command.part];
            if (!part.pending || part.enabled != command.enabled)
            {
                auto bar = static_cast<std::uint64_t>(
                               std::floor(static_cast<double>(position) / framesPerBar)) +
                           1;
                part.boundary = static_cast<std::uint64_t>(
                    std::llround(static_cast<double>(bar) * framesPerBar));
                if (part.boundary <= position)
                {
                    ++bar;
                    part.boundary = static_cast<std::uint64_t>(
                        std::llround(static_cast<double>(bar) * framesPerBar));
                }
                part.pending = true;
                part.enabled = command.enabled;
            }
            read = (read + 1) % QueueSize;
        }
        readIndex.store(read, std::memory_order_release);
    }

    float MusicPlayback::nextSample() noexcept
    {
        if (!running)
        {
            return 0;
        }
        float sample = 0;
        const auto offset = static_cast<std::size_t>(position % loopFrames);
        for (std::size_t index = 0; index < partCount; ++index)
        {
            auto& part = parts[index];
            if (part.pending && position >= part.boundary)
            {
                part.pending = false;
                part.target = part.enabled ? 1.0F : 0.0F;
                part.remaining = fadeFrames;
                if (fadeFrames == 0)
                {
                    part.gain = part.target;
                }
                else
                {
                    part.step = (part.target - part.gain) / static_cast<float>(fadeFrames);
                }
            }
            sample += part.sound->samples[offset] * part.gain;
            if (part.remaining > 0)
            {
                part.gain += part.step;
                if (--part.remaining == 0)
                {
                    part.gain = part.target;
                }
            }
        }
        ++position;
        return sample;
    }

    void MusicPlayback::mix(std::span<float> output) noexcept
    {
        consumeCommands();
        for (float& sample : output)
        {
            sample += nextSample();
        }
    }

    void MusicPlayback::stop() noexcept
    {
        running = false;
    }
}
