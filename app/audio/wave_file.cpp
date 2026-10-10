#include "audio/wave_file.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <ostream>
#include <stdexcept>

#include "advanced_platformer/audio/sound_patch.hpp"

namespace advanced_platformer
{
    namespace
    {
        void writeLittleEndian(std::ostream& output, std::uint32_t value, int bytes)
        {
            for (int index = 0; index < bytes; ++index)
            {
                output.put(static_cast<char>((value >> (index * 8)) & 0xffU));
            }
        }
    }

    void writeWaveFile(const std::filesystem::path& path, const SoundBuffer& sound)
    {
        if (sound.sampleRate <= 0 || sound.sampleRate > std::numeric_limits<int>::max() / 4 ||
            sound.samples.empty() ||
            sound.samples.size() > (std::numeric_limits<std::uint32_t>::max() - 48U) / 4U)
        {
            throw std::invalid_argument("cannot export an empty or oversized sound buffer");
        }
        for (const float sample : sound.samples)
        {
            if (!std::isfinite(sample))
            {
                throw std::invalid_argument("cannot export non-finite samples");
            }
        }
        std::ofstream output(path, std::ios::binary);
        if (!output)
        {
            throw std::runtime_error("cannot open WAV output " + path.string());
        }
        const auto bytes = static_cast<std::uint32_t>(sound.samples.size() * 4);
        output.write("RIFF", 4);
        writeLittleEndian(output, 48 + bytes, 4);
        output.write("WAVEfmt ", 8);
        writeLittleEndian(output, 16, 4);
        writeLittleEndian(output, 3, 2);
        writeLittleEndian(output, 1, 2);
        writeLittleEndian(output, static_cast<std::uint32_t>(sound.sampleRate), 4);
        writeLittleEndian(output, static_cast<std::uint32_t>(sound.sampleRate) * 4, 4);
        writeLittleEndian(output, 4, 2);
        writeLittleEndian(output, 32, 2);
        output.write("fact", 4);
        writeLittleEndian(output, 4, 4);
        writeLittleEndian(output, static_cast<std::uint32_t>(sound.samples.size()), 4);
        output.write("data", 4);
        writeLittleEndian(output, bytes, 4);
        for (const float sample : sound.samples)
        {
            writeLittleEndian(output, std::bit_cast<std::uint32_t>(sample), 4);
        }
        output.flush();
        if (!output)
        {
            throw std::runtime_error("cannot write WAV output " + path.string());
        }
    }
}
