#include "asset_watcher.hpp"

#include <filesystem>
#include <system_error>
#include <utility>

namespace advanced_platformer
{
    AssetWatcher::AssetWatcher(std::filesystem::path directory)
        : directory(std::move(directory)),
          seen(read())
    {
    }

    bool AssetWatcher::poll()
    {
        Snapshot current = read();
        if (current != seen)
        {
            seen = std::move(current);
            changing = true;
            return false;
        }
        const bool settled = changing;
        changing = false;
        return settled;
    }

    AssetWatcher::Snapshot AssetWatcher::read() const
    {
        Snapshot snapshot;
        std::error_code error;
        std::filesystem::recursive_directory_iterator entry(
            directory, std::filesystem::directory_options::skip_permission_denied, error);
        for (; !error && entry != std::filesystem::recursive_directory_iterator();
             entry.increment(error))
        {
            std::error_code fileError;
            if (!entry->is_regular_file(fileError))
            {
                continue;
            }
            const auto modified = entry->last_write_time(fileError);
            const auto size = entry->file_size(fileError);
            if (!fileError)
            {
                snapshot.emplace(entry->path(), std::pair{modified, size});
            }
        }
        return snapshot;
    }
}
