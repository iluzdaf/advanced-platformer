#include "music_song.hpp"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <algorithm>

#include "advanced_platformer/audio/sound_patch.hpp"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "advanced_platformer/audio/music.hpp"
#include "content_diagnostics.hpp"
#include "content_glaze.hpp"

namespace advanced_platformer
{
    struct MusicPhraseJson
    {
        std::vector<WithDefaults<MusicNote>> notes;
    };

    struct MusicSongJson
    {
        double tempo = 120;
        std::map<std::string, WithDefaults<MusicInstrument>, std::less<>> instruments;
        std::map<std::string, std::string, std::less<>> tracks;
        std::map<std::string, MusicPhraseJson, std::less<>> phrases;
        std::map<std::string, MusicPattern, std::less<>> patterns;
        std::vector<std::string> arrangement;
    };

    struct BackgroundMusicJson
    {
        std::optional<std::string> song;
    };

    std::shared_ptr<const SoundBuffer> loadBackgroundMusic(
        const std::filesystem::path& assetDirectory)
    {
        const auto path = assetDirectory / "catalogs" / "music.json";
        const auto settings =
            readContent<BackgroundMusicJson>(loadContentText(path), path.string());
        if (!settings.song)
        {
            return {};
        }
        if (settings.song->empty() || !std::ranges::all_of(
                                          *settings.song,
                                          [](char character)
                                          {
                                              return (character >= 'a' && character <= 'z') ||
                                                     (character >= 'A' && character <= 'Z') ||
                                                     (character >= '0' && character <= '9') ||
                                                     character == '-' || character == '_';
                                          }))
        {
            failJson(
                path.string(),
                "song",
                "use a song filename stem containing letters, digits, - or _");
        }
        return std::make_shared<const SoundBuffer>(
            renderMusic(loadMusicSong(assetDirectory / "music" / (*settings.song + ".json"))));
    }

    MusicSong parseMusicSong(std::string_view text, std::string_view sourceName)
    {
        auto json = readContent<MusicSongJson>(text, sourceName);
        MusicSong song;
        song.tempo = json.tempo;
        song.tracks = std::move(json.tracks);
        song.patterns = std::move(json.patterns);
        song.arrangement = std::move(json.arrangement);
        for (auto& [name, instrument] : json.instruments)
        {
            song.instruments.emplace(name, std::move(instrument.get()));
        }
        for (auto& [name, phrase] : json.phrases)
        {
            auto& notes = song.phrases[name].notes;
            for (auto& note : phrase.notes)
            {
                notes.push_back(std::move(note.get()));
            }
        }
        validateInFile(sourceName, [&song] { validateMusicSong(song); });
        return song;
    }

    MusicSong loadMusicSong(const std::filesystem::path& path)
    {
        return parseMusicSong(loadContentText(path), path.string());
    }
}
