#pragma once

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

namespace tests
{
    class TemporaryDirectory
    {
    public:
        TemporaryDirectory()
            : path(
                  std::filesystem::temp_directory_path() /
                  std::format(
                      "advanced_platformer_{}",
                      std::chrono::steady_clock::now().time_since_epoch().count()))
        {
            std::filesystem::create_directories(path);
        }

        ~TemporaryDirectory()
        {
            std::filesystem::remove_all(path);
        }

        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

        std::filesystem::path path;
    };

    inline void writeFile(const std::filesystem::path& file, const std::string& text)
    {
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file) << text;
    }
}
