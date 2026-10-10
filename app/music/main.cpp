#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "advanced_platformer/audio/music.hpp"
#include "advanced_platformer/audio/sound_patch.hpp"
#include "audio/audio_device.hpp"
#include "audio/wave_file.hpp"
#include "content/music_song.hpp"

namespace advanced_platformer
{
    namespace
    {
        volatile std::sig_atomic_t interrupted = 0;

        struct MusicOptions
        {
            std::filesystem::path source;
            std::filesystem::path output;
            std::string solo;
            std::string pattern;
            bool loop = false;
            bool watch = false;
        };
    }

    namespace
    {
        MusicOptions parseOptions(int argc, const char* const* argv)
        {
            MusicOptions options;
            for (int index = 1; index < argc; ++index)
            {
                const std::string_view argument = argv[index];
                if (argument == "--loop")
                {
                    options.loop = true;
                }
                else if (argument == "--watch")
                {
                    options.watch = true;
                    options.loop = true;
                }
                else if (argument == "--solo" || argument == "--pattern" || argument == "--output")
                {
                    if (++index == argc)
                    {
                        throw std::invalid_argument("missing value for " + std::string(argument));
                    }
                    if (argument == "--solo")
                    {
                        options.solo = argv[index];
                    }
                    else if (argument == "--pattern")
                    {
                        options.pattern = argv[index];
                    }
                    else
                    {
                        options.output = argv[index];
                    }
                }
                else if (argument.starts_with('-') || !options.source.empty())
                {
                    throw std::invalid_argument("unexpected argument " + std::string(argument));
                }
                else
                {
                    options.source = argument;
                }
            }
            if (options.source.empty() || (!options.output.empty() && options.loop))
            {
                throw std::invalid_argument(
                    "provide a song JSON; --output cannot be combined with --loop or --watch");
            }
            return options;
        }
    }

    namespace
    {
        std::shared_ptr<const SoundBuffer> prepareMusic(const MusicOptions& options)
        {
            auto song = loadMusicSong(options.source);
            if (!options.pattern.empty())
            {
                song.arrangement = {options.pattern};
            }
            auto sound = std::make_shared<const SoundBuffer>(renderMusic(song, options.solo));
            std::cout << "Prepared " << options.source.string() << ": "
                      << static_cast<double>(sound->samples.size()) / sound->sampleRate
                      << " seconds";
            if (!options.solo.empty())
            {
                std::cout << ", solo " << options.solo;
            }
            std::cout << '\n';
            return sound;
        }
    }

    namespace
    {
        void interruptMusic(int)
        {
            interrupted = 1;
        }

        int auditionMusic(const MusicOptions& options)
        {
            auto sound = prepareMusic(options);
            if (!options.output.empty())
            {
                writeWaveFile(options.output, *sound);
                std::cout << "Wrote " << options.output.string() << '\n';
                return EXIT_SUCCESS;
            }
            AudioDevice device;
            if (!device.play(sound, options.loop))
            {
                throw std::runtime_error("cannot queue music playback");
            }
            std::weak_ptr<const SoundBuffer> playing = sound;
            sound.reset();
            auto modified = std::filesystem::last_write_time(options.source);
            std::signal(SIGINT, interruptMusic);
            std::signal(SIGTERM, interruptMusic);
            if (options.loop)
            {
                std::cout << "Looping; press Ctrl-C to stop.\n";
            }
            while (!interrupted)
            {
                device.collectFinished();
                if (!options.loop && playing.expired())
                {
                    break;
                }
                if (options.watch)
                {
                    try
                    {
                        const auto current = std::filesystem::last_write_time(options.source);
                        if (current != modified)
                        {
                            modified = current;
                            auto replacement = prepareMusic(options);
                            if (!device.replace(std::move(replacement), true))
                            {
                                throw std::runtime_error("cannot queue music reload");
                            }
                            std::cout << "Reloaded from the beginning.\n";
                        }
                    }
                    catch (const std::exception& error)
                    {
                        std::cerr << "Reload rejected: " << error.what() << '\n';
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            return EXIT_SUCCESS;
        }
    }

    int runMusicTool(int argc, const char* const* argv)
    {
        if (argc == 2 && std::string_view(argv[1]) == "--help")
        {
            std::cout
                << "Usage: advanced_platformer_music song.json [--solo track] [--pattern name]\n"
                   "       [--loop | --watch | --output preview.wav]\n";
            return EXIT_SUCCESS;
        }
        return auditionMusic(parseOptions(argc, argv));
    }
}

int main(int argc, char** argv)
{
    try
    {
        return advanced_platformer::runMusicTool(argc, argv);
    }
    catch (const std::exception& error)
    {
        std::cerr << "Music audition failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
