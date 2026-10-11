#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "advanced_platformer/audio/sound_patch.hpp"

namespace advanced_platformer
{
    struct MusicInstrument
    {
        std::string wave = "sine";
        double attack = 0.01;
        double decay = 0.08;
        double sustain = 0.65;
        double release = 0.08;
        double gain = 0.2;
    };

    struct MusicNote
    {
        std::string pitch;
        double beats = 0;
        std::optional<double> gate;
        double volume = 1;
    };

    struct MusicPhrase
    {
        std::vector<MusicNote> notes;
    };

    struct MusicPattern
    {
        double beats = 0;
        std::map<std::string, std::string, std::less<>> tracks;
    };

    struct MusicSong
    {
        double tempo = 120;
        std::map<std::string, MusicInstrument, std::less<>> instruments;
        std::map<std::string, std::string, std::less<>> tracks;
        std::map<std::string, MusicPhrase, std::less<>> phrases;
        std::map<std::string, MusicPattern, std::less<>> patterns;
        std::vector<std::string> arrangement;
    };

    double musicPitchFrequency(std::string_view pitch);
    void validateMusicSong(const MusicSong& song);
    SoundBuffer renderMusic(const MusicSong& song, std::string_view soloTrack = {});
}
