#include "advanced_platformer/audio/audio_playback.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <string_view>

#include "advanced_platformer/audio/music_playback.hpp"

#include "advanced_platformer/audio/sound_patch.hpp"

namespace advanced_platformer
{
    static_assert(std::atomic<std::size_t>::is_always_lock_free);
    static_assert(std::atomic<bool>::is_always_lock_free);

    AudioPlayback::AudioPlayback()
        : AudioPlayback(nullptr)
    {
    }

    AudioPlayback::AudioPlayback(std::unique_ptr<MusicPlayback> music)
        : musicParts(std::move(music))
    {
        for (auto& flag : finished)
        {
            flag.store(true, std::memory_order_relaxed);
        }
    }

    AudioPlayback::~AudioPlayback() = default;

    bool AudioPlayback::setPartActive(std::string_view track, bool enabled)
    {
        return musicParts && musicParts->setPartActive(track, enabled);
    }

    void AudioPlayback::collectFinished()
    {
        for (std::size_t slot = 0; slot < retained.size(); ++slot)
        {
            if (finished[slot].load(std::memory_order_acquire))
            {
                retained[slot].reset();
            }
        }
    }

    bool AudioPlayback::play(std::shared_ptr<const SoundBuffer> sound, bool loop)
    {
        return enqueue(std::move(sound), loop, CommandKind::Play);
    }

    bool AudioPlayback::replace(std::shared_ptr<const SoundBuffer> sound, bool loop)
    {
        return enqueue(std::move(sound), loop, CommandKind::Replace);
    }

    bool AudioPlayback::setMusic(std::shared_ptr<const SoundBuffer> sound)
    {
        return enqueue(std::move(sound), true, CommandKind::Music);
    }

    bool AudioPlayback::enqueue(
        std::shared_ptr<const SoundBuffer> sound,
        bool loop,
        CommandKind kind)
    {
        collectFinished();
        if ((!sound && kind != CommandKind::Music) ||
            (sound && (sound->samples.empty() || sound->sampleRate <= 0)))
        {
            return false;
        }
        const std::size_t write = writeIndex.load(std::memory_order_relaxed);
        const std::size_t next = (write + 1) % QueueSize;
        if (next == readIndex.load(std::memory_order_acquire))
        {
            return false;
        }
        if (!sound)
        {
            commands[write] = {.kind = CommandKind::Music};
            writeIndex.store(next, std::memory_order_release);
            return true;
        }
        for (std::size_t slot = 0; slot < retained.size(); ++slot)
        {
            if (retained[slot])
            {
                continue;
            }
            retained[slot] = std::move(sound);
            finished[slot].store(false, std::memory_order_relaxed);
            commands[write] = {retained[slot].get(), slot, loop, kind};
            writeIndex.store(next, std::memory_order_release);
            return true;
        }
        return false;
    }

    bool AudioPlayback::stop()
    {
        const std::size_t write = writeIndex.load(std::memory_order_relaxed);
        const std::size_t next = (write + 1) % QueueSize;
        if (next == readIndex.load(std::memory_order_acquire))
        {
            return false;
        }
        commands[write] = {};
        writeIndex.store(next, std::memory_order_release);
        return true;
    }

    void AudioPlayback::render(std::span<float> output) noexcept
    {
        std::size_t read = readIndex.load(std::memory_order_relaxed);
        const std::size_t write = writeIndex.load(std::memory_order_acquire);
        while (read != write)
        {
            const Command command = commands[read];
            if (command.kind == CommandKind::Music)
            {
                if (musicSlot)
                {
                    mixer.stop(*musicSlot);
                    active[*musicSlot] = false;
                    finished[*musicSlot].store(true, std::memory_order_release);
                    musicSlot.reset();
                }
                if (command.sound != nullptr)
                {
                    musicSlot = command.slot;
                }
            }
            else if (command.kind == CommandKind::Stop || command.kind == CommandKind::Replace)
            {
                mixer.stop();
                if (musicParts)
                {
                    musicParts->stop();
                }
                musicSlot.reset();
                for (std::size_t slot = 0; slot < active.size(); ++slot)
                {
                    if (active[slot])
                    {
                        active[slot] = false;
                        finished[slot].store(true, std::memory_order_release);
                    }
                }
            }
            if (command.sound != nullptr)
            {
                mixer.play(command.slot, *command.sound, command.loop);
                active[command.slot] = true;
            }
            read = (read + 1) % QueueSize;
        }
        readIndex.store(read, std::memory_order_release);
        mixer.render(output);
        if (musicParts)
        {
            musicParts->mix(output);
            for (float& sample : output)
            {
                sample = std::clamp(sample, -1.0F, 1.0F);
            }
        }
        for (std::size_t slot = 0; slot < active.size(); ++slot)
        {
            if (active[slot] && !mixer.playing(slot))
            {
                active[slot] = false;
                finished[slot].store(true, std::memory_order_release);
            }
        }
    }
}
