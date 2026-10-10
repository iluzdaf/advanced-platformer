#include "sound_catalog.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <stdexcept>

#include <glaze/glaze.hpp>

#include "advanced_platformer/audio/sound_patch.hpp"
#include "content_diagnostics.hpp"
#include "content_glaze.hpp"

// NOLINTBEGIN(readability-identifier-naming)
template <> struct glz::meta<advanced_platformer::SoundPatch>
{
    using T = advanced_platformer::SoundPatch;
    static constexpr auto value = glz::object(
        "wave_type",
        &T::waveType,
        "p_env_attack",
        &T::attack,
        "p_env_sustain",
        &T::sustain,
        "p_env_punch",
        &T::punch,
        "p_env_decay",
        &T::decay,
        "p_base_freq",
        &T::baseFrequency,
        "p_freq_limit",
        &T::frequencyLimit,
        "p_freq_ramp",
        &T::frequencySlide,
        "p_freq_dramp",
        &T::deltaSlide,
        "p_vib_strength",
        &T::vibratoDepth,
        "p_vib_speed",
        &T::vibratoSpeed,
        "p_arp_mod",
        &T::changeAmount,
        "p_arp_speed",
        &T::changeSpeed,
        "p_duty",
        &T::duty,
        "p_duty_ramp",
        &T::dutySweep,
        "p_repeat_speed",
        &T::repeatSpeed,
        "p_pha_offset",
        &T::phaserOffset,
        "p_pha_ramp",
        &T::phaserSweep,
        "p_lpf_freq",
        &T::lowPassCutoff,
        "p_lpf_ramp",
        &T::lowPassSweep,
        "p_lpf_resonance",
        &T::lowPassResonance,
        "p_hpf_freq",
        &T::highPassCutoff,
        "p_hpf_ramp",
        &T::highPassSweep,
        "sound_vol",
        &T::volume,
        "sample_rate",
        &T::sampleRate,
        "sample_size",
        &T::sampleSize,
        "oldParams",
        &T::oldParams);
};

// NOLINTEND(readability-identifier-naming)

namespace advanced_platformer
{
    struct SoundsJson
    {
        std::map<std::string, WithDefaults<SoundPatch>> sounds;
    };

    SoundCatalog parseSoundCatalog(std::string_view text, std::string_view sourceName)
    {
        const auto file = readContent<SoundsJson>(text, sourceName);
        SoundCatalog result;
        for (const auto& [name, parameters] : file.sounds)
        {
            const std::string path = fieldPath("sounds", name);
            if (name.empty())
            {
                failJson(sourceName, path, "a sound needs a non-empty name");
            }
            try
            {
                result.emplace(
                    name, std::make_shared<const SoundBuffer>(renderSound(parameters.get())));
            }
            catch (const std::invalid_argument& error)
            {
                failJson(sourceName, path, error.what());
            }
        }
        return result;
    }

    SoundCatalog loadSoundCatalog(const std::filesystem::path& path)
    {
        return parseSoundCatalog(loadContentText(path), path.string());
    }
}
