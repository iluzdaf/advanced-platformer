#include "content_diagnostics.hpp"

#include <cstddef>
#include <format>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace advanced_platformer
{
    std::string fieldPath(std::string_view path, std::string_view key)
    {
        if (path.empty())
        {
            return std::string(key);
        }
        return std::format("{}.{}", path, key);
    }

    std::string indexPath(std::string_view path, std::size_t index)
    {
        return std::format("{}[{}]", path, index);
    }

    void validateInFile(std::string_view sourceName, const std::function<void()>& validate)
    {
        try
        {
            validate();
        }
        catch (const std::invalid_argument& error)
        {
            failJson(sourceName, {}, error.what());
        }
    }

    void failJson(std::string_view sourceName, std::string_view path, std::string_view message)
    {
        std::string prefix;
        if (!sourceName.empty())
        {
            prefix += std::format("{}: ", sourceName);
        }
        if (!path.empty())
        {
            prefix += std::format("{}: ", path);
        }
        throw std::invalid_argument(std::format("{}{}", prefix, message));
    }
}
