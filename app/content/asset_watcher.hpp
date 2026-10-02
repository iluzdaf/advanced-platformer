#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <utility>

namespace advanced_platformer
{
    class AssetWatcher
    {
    public:
        explicit AssetWatcher(std::filesystem::path directory);

        bool poll();

    private:
        using Snapshot = std::
            map<std::filesystem::path, std::pair<std::filesystem::file_time_type, std::uintmax_t>>;

        Snapshot read() const;

        std::filesystem::path directory;
        Snapshot seen;
        bool changing = false;
    };
}
