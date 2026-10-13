#include <array>
#include <cerrno>
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
#include <sstream>
#include <sys/poll.h>
#include <unistd.h>
#include <thread>
#include <utility>

#include "advanced_platformer/audio/music.hpp"
#include "advanced_platformer/audio/sound_patch.hpp"
#include "advanced_platformer/audio/audio_device.hpp"
#include "advanced_platformer/audio/music_playback.hpp"
#include "wave_file.hpp"
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
            std::string backing;
            bool loop = false;
            bool watch = false;
        };

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
                else if (
                    argument == "--solo" || argument == "--pattern" || argument == "--output" ||
                    argument == "--parts")
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
                    else if (argument == "--parts")
                    {
                        options.backing = argv[index];
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
            if (!options.backing.empty() &&
                (!options.solo.empty() || !options.output.empty() || options.watch))
            {
                throw std::invalid_argument(
                    "--parts cannot be combined with --solo, --output, or --watch");
            }
            return options;
        }

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

        void interruptMusic(int)
        {
            interrupted = 1;
        }

        bool controlParts(AudioDevice& device, const std::string& line)
        {
            std::istringstream input(line);
            std::string command;
            std::string track;
            std::string extra;
            input >> command;
            if (command.empty())
            {
                return true;
            }
            if (command == "quit" && !(input >> extra))
            {
                return false;
            }
            if ((command != "on" && command != "off") || !(input >> track) || (input >> extra))
            {
                std::cerr << "Use on <track>, off <track>, or quit.\n";
            }
            else if (!device.setPartActive(track, command == "on"))
            {
                std::cerr << "Request rejected: unknown track, backing track, or full queue.\n";
            }
            else
            {
                std::cout << track << " queued " << command << " for the next bar.\n" << std::flush;
            }
            return true;
        }

        int auditionParts(const MusicOptions& options)
        {
            auto song = loadMusicSong(options.source);
            if (!options.pattern.empty())
            {
                song.arrangement = {options.pattern};
            }
            auto prepared = renderMusicParts(song);
            std::cout << "Backing: " << options.backing << "; muted parts:";
            for (const auto& [name, sound] : prepared.tracks)
            {
                if (name != options.backing)
                {
                    std::cout << ' ' << name;
                }
            }
            AudioDevice device(
                std::make_unique<MusicPlayback>(std::move(prepared), options.backing));
            std::signal(SIGINT, interruptMusic);
            std::signal(SIGTERM, interruptMusic);
            std::cout << "\nUse on <track>, off <track>, or quit. Changes fade over 50 ms at the "
                         "next four-beat bar.\n"
                      << std::flush;
            std::string pending;
            bool running = true;
            while (running && !interrupted)
            {
                pollfd descriptor{STDIN_FILENO, POLLIN, 0};
                const int ready = poll(&descriptor, 1, 20);
                if (ready < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    throw std::runtime_error("cannot poll music controls");
                }
                if (ready == 0)
                {
                    continue;
                }
                std::array<char, 256> buffer{};
                const auto count = read(STDIN_FILENO, buffer.data(), buffer.size());
                if (count < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    throw std::runtime_error("cannot read music controls");
                }
                if (count == 0)
                {
                    if (!pending.empty())
                    {
                        controlParts(device, pending);
                    }
                    break;
                }
                pending.append(buffer.data(), static_cast<std::size_t>(count));
                std::size_t end = 0;
                while (running && (end = pending.find('\n')) != std::string::npos)
                {
                    running = controlParts(device, pending.substr(0, end));
                    pending.erase(0, end + 1);
                }
                if (pending.size() > 4096)
                {
                    throw std::invalid_argument("music control line is too long");
                }
            }
            return EXIT_SUCCESS;
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
                   "       [--loop | --watch | --output preview.wav | --parts backing-track]\n"
                   "Parts mode: on <track>, off <track>, quit; synchronized four-beat bars.\n";
            return EXIT_SUCCESS;
        }
        const auto options = parseOptions(argc, argv);
        return options.backing.empty() ? auditionMusic(options) : auditionParts(options);
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
