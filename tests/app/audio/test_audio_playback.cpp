#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <utility>

#include "audio/audio_playback.hpp"
#include "advanced_platformer/audio/sound_mixer.hpp"
#include "advanced_platformer/audio/sound_patch.hpp"

TEST_CASE(
    "Audio playback retains a sound until the callback finishes and the producer collects it",
    "[audio][playback]")
{
    advanced_platformer::AudioPlayback playback;
    auto sound = std::make_shared<const advanced_platformer::SoundBuffer>(
        advanced_platformer::SoundBuffer{{0.1F, 0.2F, 0.3F}, 44100});
    const std::weak_ptr<const advanced_platformer::SoundBuffer> lifetime = sound;
    const bool queued = playback.play(std::move(sound));
    REQUIRE(queued);
    std::array<float, 2> output{};
    playback.render(output);
    REQUIRE(output == std::array{0.1F, 0.2F});
    playback.collectFinished();
    REQUIRE_FALSE(lifetime.expired());
    playback.render(output);
    REQUIRE(output == std::array{0.3F, 0.0F});
    REQUIRE_FALSE(lifetime.expired());
    playback.collectFinished();
    REQUIRE(lifetime.expired());
}

TEST_CASE(
    "Loop playback keeps its buffer until a stop is consumed and collected",
    "[audio][playback]")
{
    advanced_platformer::AudioPlayback playback;
    auto sound = std::make_shared<const advanced_platformer::SoundBuffer>(
        advanced_platformer::SoundBuffer{{0.1F, 0.2F}, 44100});
    const std::weak_ptr<const advanced_platformer::SoundBuffer> lifetime = sound;
    const bool queued = playback.play(std::move(sound), true);
    REQUIRE(queued);
    std::array<float, 3> output{};
    playback.render(output);
    REQUIRE(output == std::array{0.1F, 0.2F, 0.1F});
    playback.collectFinished();
    REQUIRE_FALSE(lifetime.expired());
    REQUIRE(playback.stop());
    REQUIRE_FALSE(lifetime.expired());
    playback.render(output);
    REQUIRE(output == std::array{0.0F, 0.0F, 0.0F});
    REQUIRE_FALSE(lifetime.expired());
    playback.collectFinished();
    REQUIRE(lifetime.expired());
}

TEST_CASE(
    "Concurrent audio playback releases every buffer on the producer thread",
    "[audio][playback][thread]")
{
    advanced_platformer::AudioPlayback playback;
    std::atomic<bool> stop{false};
    std::atomic<bool> wrongThread{false};
    std::atomic<int> released{0};
    const std::thread::id producer = std::this_thread::get_id();
    std::thread callback(
        [&]
        {
            std::array<float, 64> output{};
            while (!stop.load(std::memory_order_acquire))
            {
                playback.render(output);
                std::this_thread::yield();
            }
            playback.render(output);
        });
    for (int index = 0; index < 1000; ++index)
    {
        std::shared_ptr<const advanced_platformer::SoundBuffer> sound(
            new advanced_platformer::SoundBuffer{{0.1F, 0.2F}, 44100},
            [&](const advanced_platformer::SoundBuffer* value)
            {
                if (std::this_thread::get_id() != producer)
                {
                    wrongThread.store(true, std::memory_order_relaxed);
                }
                released.fetch_add(1, std::memory_order_relaxed);
                delete value;
            });
        while (!playback.play(sound))
        {
            std::this_thread::yield();
        }
    }
    stop.store(true, std::memory_order_release);
    callback.join();
    playback.collectFinished();
    REQUIRE_FALSE(wrongThread.load());
    REQUIRE(released.load() == 1000);
}

TEST_CASE("Reloaded sound buffers cannot invalidate a playing voice", "[audio][playback][reload]")
{
    advanced_platformer::AudioPlayback playback;
    auto sound = std::make_shared<const advanced_platformer::SoundBuffer>(
        advanced_platformer::SoundBuffer{{0.1F, 0.2F}, 44100});
    REQUIRE(playback.play(sound));
    sound = std::make_shared<const advanced_platformer::SoundBuffer>(
        advanced_platformer::SoundBuffer{{0.4F, 0.5F}, 44100});
    REQUIRE(playback.play(sound));
    std::array<float, 2> output{};
    playback.render(output);
    REQUIRE(output == std::array{0.5F, 0.7F});
}

TEST_CASE(
    "Stop and replacement commands preserve order within a callback",
    "[audio][playback][reload]")
{
    advanced_platformer::AudioPlayback playback;
    const auto oldSound = std::make_shared<const advanced_platformer::SoundBuffer>(
        advanced_platformer::SoundBuffer{{0.1F}, 44100});
    const auto newSound = std::make_shared<const advanced_platformer::SoundBuffer>(
        advanced_platformer::SoundBuffer{{0.4F, 0.5F}, 44100});
    REQUIRE(playback.play(oldSound, true));
    REQUIRE(playback.stop());
    REQUIRE(playback.play(newSound, true));
    std::array<float, 3> output{};
    playback.render(output);
    REQUIRE(output == std::array{0.4F, 0.5F, 0.4F});
    REQUIRE(playback.stop());
    playback.render(output);
    playback.collectFinished();
    REQUIRE(playback.play(newSound));
}

TEST_CASE(
    "A replacement changes loops together and preserves the old buffer until collection",
    "[audio][playback][reload]")
{
    advanced_platformer::AudioPlayback playback;
    auto oldSound = std::make_shared<const advanced_platformer::SoundBuffer>(
        advanced_platformer::SoundBuffer{{0.1F}, 44100});
    const std::weak_ptr<const advanced_platformer::SoundBuffer> oldLifetime = oldSound;
    const bool queued = playback.play(std::move(oldSound), true);
    REQUIRE(queued);
    std::array<float, 2> output{};
    playback.render(output);
    const auto replacement = std::make_shared<const advanced_platformer::SoundBuffer>(
        advanced_platformer::SoundBuffer{{0.4F, 0.5F}, 44100});
    REQUIRE(playback.replace(replacement, true));
    playback.render(output);
    REQUIRE(output == std::array{0.4F, 0.5F});
    REQUIRE_FALSE(oldLifetime.expired());
    playback.collectFinished();
    REQUIRE(oldLifetime.expired());
    REQUIRE_FALSE(playback.replace({}));
    playback.render(output);
    REQUIRE(output == std::array{0.4F, 0.5F});
}

TEST_CASE("A rejected replacement leaves the existing loop playing", "[audio][playback][reload]")
{
    advanced_platformer::AudioPlayback playback;
    const auto sound = std::make_shared<const advanced_platformer::SoundBuffer>(
        advanced_platformer::SoundBuffer{{0.1F}, 44100});
    REQUIRE(playback.play(sound, true));
    std::array<float, 1> output{};
    playback.render(output);
    for (std::size_t index = 1; index < advanced_platformer::SoundVoiceCount; ++index)
    {
        REQUIRE(playback.play(sound, true));
    }
    REQUIRE_FALSE(playback.replace(sound, true));
    playback.render(output);
    REQUIRE(output.front() == 1.0F);
}

TEST_CASE(
    "The bounded audio queue refuses excess voices and reuses completed slots",
    "[audio][playback]")
{
    advanced_platformer::AudioPlayback playback;
    const auto sound = std::make_shared<const advanced_platformer::SoundBuffer>(
        advanced_platformer::SoundBuffer{{0.01F}, 44100});
    for (std::size_t index = 0; index < advanced_platformer::SoundVoiceCount; ++index)
    {
        REQUIRE(playback.play(sound));
    }
    REQUIRE_FALSE(playback.play(sound));
    std::array<float, 1> output{};
    playback.render(output);
    REQUIRE(playback.play(sound));
    playback.render(output);
    REQUIRE(output[0] == 0.01F);
    REQUIRE_FALSE(playback.play({}));
}

TEST_CASE("Stop refuses a full command queue and succeeds after consumption", "[audio][playback]")
{
    advanced_platformer::AudioPlayback playback;
    for (std::size_t index = 0; index < advanced_platformer::SoundVoiceCount; ++index)
    {
        REQUIRE(playback.stop());
    }
    REQUIRE_FALSE(playback.stop());
    std::array<float, 1> output{};
    playback.render(output);
    REQUIRE(playback.stop());
}
