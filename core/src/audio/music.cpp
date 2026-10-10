#include "advanced_platformer/audio/music.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>

#include "advanced_platformer/audio/sound_mixer.hpp"
#include "advanced_platformer/audio/sound_patch.hpp"

namespace advanced_platformer
{
    namespace
    {
        constexpr double MaximumSongSeconds = 120;

        std::size_t sampleAt(double seconds)
        {
            return static_cast<std::size_t>(std::llround(seconds * SoundOutputRate));
        }
    }

    double musicPitchFrequency(std::string_view pitch)
    {
        if (pitch.size() < 2 || pitch.size() > 3 || pitch.front() < 'A' || pitch.front() > 'G')
        {
            throw std::invalid_argument(
                "pitch needs a letter A..G and octave 0..9, with optional # or b");
        }
        const char octave = pitch.back();
        if (octave < '0' || octave > '9')
        {
            throw std::invalid_argument("pitch needs an octave from 0 to 9");
        }
        constexpr int Semitones[]{9, 11, 0, 2, 4, 5, 7};
        int semitone = Semitones[pitch.front() - 'A'];
        if (pitch.size() == 3)
        {
            if (pitch[1] != '#' && pitch[1] != 'b')
            {
                throw std::invalid_argument("pitch accidental must be # or b");
            }
            semitone += pitch[1] == '#' ? 1 : -1;
        }
        const int midi = (octave - '0' + 1) * 12 + semitone;
        const double frequency = 440 * std::exp2(static_cast<double>(midi - 69) / 12);
        if (frequency >= SoundOutputRate / 2.0)
        {
            throw std::invalid_argument("pitch exceeds the output frequency limit");
        }
        return frequency;
    }

    namespace
    {
        void requireRange(double value, double minimum, double maximum, const std::string& path)
        {
            if (!std::isfinite(value) || value < minimum || value > maximum)
            {
                throw std::invalid_argument(path + ": value is outside its allowed range");
            }
        }

    }

    void validateMusicSong(const MusicSong& song)
    {
        requireRange(song.tempo, 20, 400, "tempo");
        if (song.arrangement.empty())
        {
            throw std::invalid_argument("arrangement needs at least one pattern");
        }
        for (const auto& [name, instrument] : song.instruments)
        {
            const std::string path = "instruments." + name;
            if (name.empty() || (instrument.wave != "sine" && instrument.wave != "triangle" &&
                                 instrument.wave != "square" && instrument.wave != "noise"))
            {
                throw std::invalid_argument(
                    path + ": use a named sine, triangle, square, or noise instrument");
            }
            requireRange(instrument.attack, 0, 5, path + ".attack");
            requireRange(instrument.decay, 0, 5, path + ".decay");
            requireRange(instrument.sustain, 0, 1, path + ".sustain");
            requireRange(instrument.release, 0, 5, path + ".release");
            requireRange(instrument.gain, 0, 1, path + ".gain");
        }
        if (song.tracks.empty() || song.tracks.size() > SoundVoiceCount)
        {
            throw std::invalid_argument("tracks needs between 1 and 32 named tracks");
        }
        for (const auto& [track, instrument] : song.tracks)
        {
            if (track.empty() || !song.instruments.contains(instrument))
            {
                throw std::invalid_argument(
                    std::format("tracks.{}: unknown instrument {}", track, instrument));
            }
        }
        for (const auto& [name, phrase] : song.phrases)
        {
            if (name.empty() || phrase.notes.empty())
            {
                throw std::invalid_argument("phrases." + name + ": needs a name and notes");
            }
            double beats = 0;
            for (const auto& note : phrase.notes)
            {
                const std::string path = "phrases." + name + ".notes[" +
                                         std::to_string(&note - phrase.notes.data()) + "]";
                requireRange(note.beats, 1.0 / 64, 64, path + ".beats");
                requireRange(note.volume, 0, 1, path + ".volume");
                if (note.gate)
                {
                    requireRange(*note.gate, 1.0 / 64, note.beats, path + ".gate");
                }
                if (note.pitch != "rest")
                {
                    try
                    {
                        static_cast<void>(musicPitchFrequency(note.pitch));
                    }
                    catch (const std::invalid_argument& error)
                    {
                        throw std::invalid_argument(path + ".pitch: " + error.what());
                    }
                }
                else if (note.gate)
                {
                    throw std::invalid_argument(path + ".gate: a rest cannot have a gate");
                }
                beats += note.beats;
            }
            requireRange(beats, 1.0 / 64, MaximumSongSeconds * song.tempo / 60, "phrases." + name);
        }
        for (const auto& [name, pattern] : song.patterns)
        {
            const std::string path = "patterns." + name;
            if (name.empty() || pattern.tracks.empty())
            {
                throw std::invalid_argument(path + ": needs a name and tracks");
            }
            requireRange(pattern.beats, 1.0 / 64, 256, path + ".beats");
            for (const auto& [track, phraseName] : pattern.tracks)
            {
                if (!song.tracks.contains(track) || !song.phrases.contains(phraseName))
                {
                    throw std::invalid_argument(
                        std::format(
                            "{}.tracks.{}: unknown track or phrase {}", path, track, phraseName));
                }
                double beats = 0;
                for (const auto& note : song.phrases.at(phraseName).notes)
                {
                    beats += note.beats;
                }
                if (beats > pattern.beats + 1e-9)
                {
                    throw std::invalid_argument(
                        std::format("{}.tracks.{}: phrase exceeds pattern length", path, track));
                }
            }
        }
        double beats = 0;
        for (const auto& name : song.arrangement)
        {
            if (!song.patterns.contains(name))
            {
                throw std::invalid_argument("arrangement: unknown pattern " + name);
            }
            beats += song.patterns.at(name).beats;
        }
        requireRange(
            beats * 60 / song.tempo, 0, MaximumSongSeconds, "arrangement duration (seconds)");
    }

    namespace
    {
        double envelopeAt(const MusicInstrument& instrument, double seconds)
        {
            if (instrument.attack > 0 && seconds < instrument.attack)
            {
                return seconds / instrument.attack;
            }
            if (instrument.decay > 0 && seconds < instrument.attack + instrument.decay)
            {
                return 1 +
                       (instrument.sustain - 1) * (seconds - instrument.attack) / instrument.decay;
            }
            return instrument.sustain;
        }

    }

    namespace
    {
        void addNote(
            SoundBuffer& output,
            const MusicInstrument& instrument,
            const MusicNote& note,
            double startSeconds,
            double secondsPerBeat)
        {
            const double frequency = musicPitchFrequency(note.pitch);
            const double gate = note.gate.value_or(note.beats) * secondsPerBeat;
            const std::size_t start = sampleAt(startSeconds);
            const std::size_t end = sampleAt(startSeconds + gate + instrument.release);
            const double releaseLevel = envelopeAt(instrument, gate);
            std::uint32_t noiseState = 1;
            constexpr int Oversampling = 8;
            for (std::size_t sample = start; sample < end; ++sample)
            {
                const double seconds = static_cast<double>(sample - start) / SoundOutputRate;
                const double envelope =
                    seconds < gate
                        ? envelopeAt(instrument, seconds)
                        : (instrument.release > 0
                               ? releaseLevel *
                                     std::max(0.0, 1 - (seconds - gate) / instrument.release)
                               : 0);
                double wave = 0;
                for (int sub = 0; sub < Oversampling; ++sub)
                {
                    const double phase =
                        frequency *
                        (seconds + static_cast<double>(sub) / (SoundOutputRate * Oversampling));
                    const double fraction = phase - std::floor(phase);
                    if (instrument.wave == "sine")
                    {
                        wave += std::sin(2 * std::numbers::pi * fraction);
                    }
                    else if (instrument.wave == "triangle")
                    {
                        wave += 1 - 4 * std::abs(fraction - 0.5);
                    }
                    else if (instrument.wave == "square")
                    {
                        wave += fraction < 0.5 ? 1 : -1;
                    }
                    else
                    {
                        noiseState = noiseState * 1664525U + 1013904223U;
                        wave += 2 * static_cast<double>(noiseState) /
                                    std::numeric_limits<std::uint32_t>::max() -
                                1;
                    }
                }
                output.samples[sample % output.samples.size()] += static_cast<float>(
                    wave / Oversampling * envelope * instrument.gain * note.volume);
            }
        }
    }

    SoundBuffer renderMusic(const MusicSong& song, std::string_view soloTrack)
    {
        validateMusicSong(song);
        if (!soloTrack.empty() && !song.tracks.contains(soloTrack))
        {
            throw std::invalid_argument("unknown solo track " + std::string(soloTrack));
        }
        const double secondsPerBeat = 60 / song.tempo;
        double totalBeats = 0;
        for (const auto& name : song.arrangement)
        {
            totalBeats += song.patterns.at(name).beats;
        }
        SoundBuffer output;
        output.samples.resize(sampleAt(totalBeats * secondsPerBeat));
        double patternStart = 0;
        for (const auto& name : song.arrangement)
        {
            const auto& pattern = song.patterns.at(name);
            for (const auto& [track, phraseName] : pattern.tracks)
            {
                if (!soloTrack.empty() && track != soloTrack)
                {
                    continue;
                }
                const auto& instrument = song.instruments.at(song.tracks.at(track));
                double noteStart = patternStart;
                for (const auto& note : song.phrases.at(phraseName).notes)
                {
                    if (note.pitch != "rest")
                    {
                        addNote(
                            output, instrument, note, noteStart * secondsPerBeat, secondsPerBeat);
                    }
                    noteStart += note.beats;
                }
            }
            patternStart += pattern.beats;
        }
        for (float& sample : output.samples)
        {
            sample = std::clamp(sample, -1.0F, 1.0F);
        }
        return output;
    }
}
