#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glaze/glaze.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/render/sprite.hpp"

namespace advanced_platformer
{
    template <class Enum> struct ContentNames;

    template <std::size_t Count> std::string listOfNames(const auto& names)
    {
        std::string list;
        for (std::size_t index = 0; index < Count; ++index)
        {
            if (index > 0)
            {
                list += index + 1 == Count ? " or " : ", ";
            }
            list += names[index].first;
        }
        return list;
    }

    template <class Enum> struct NamedEnumReader
    {
        template <auto Options> static void op(Enum& value, auto&& context, auto&& it, auto&& end)
        {
            const auto start = it;
            std::string name;
            glz::parse<glz::JSON>::op<Options>(name, context, it, end);
            if (bool(context.error))
            {
                return;
            }
            constexpr auto& Names = ContentNames<Enum>::Names;
            for (const auto& [candidate, enumerator] : Names)
            {
                if (candidate == name)
                {
                    value = enumerator;
                    return;
                }
            }
            static const std::string expected = "expected " + listOfNames<Names.size()>(Names);
            it = start;
            context.error = glz::error_code::unexpected_enum;
            context.custom_error_message = expected;
        }
    };

    template <class T> class WithDefaults
    {
    public:
        T& get()
        {
            return value;
        }

        const T& get() const
        {
            return value;
        }

    private:
        T value{};
    };

    template <> struct ContentNames<SpriteAnchor>
    {
        static constexpr std::array Names{
            std::pair{std::string_view{"feet"}, SpriteAnchor::BodyFeet},
            std::pair{std::string_view{"center"}, SpriteAnchor::BodyCenter}};
    };
}

template <>
struct glz::from<glz::JSON, advanced_platformer::SpriteAnchor>
    : advanced_platformer::NamedEnumReader<advanced_platformer::SpriteAnchor>
{
};

template <class T> struct glz::from<glz::JSON, advanced_platformer::WithDefaults<T>>
{
    template <auto Options>
    static void op(
        advanced_platformer::WithDefaults<T>& wrapped,
        auto&& context,
        auto&& it,
        auto&& end)
    {
        parse<JSON>::op<opt_false<Options, &opts::error_on_missing_keys>>(
            wrapped.get(), context, it, end);
    }
};

template <> struct glz::from<glz::JSON, glm::vec2>
{
    template <auto Options> static void op(glm::vec2& value, auto&& context, auto&& it, auto&& end)
    {
        const auto start = it;
        std::vector<float> numbers;
        parse<JSON>::op<Options>(numbers, context, it, end);
        if (bool(context.error))
        {
            return;
        }
        if (numbers.size() != 2)
        {
            it = start;
            context.error = error_code::syntax_error;
            context.custom_error_message = "expected two numbers, [x, y]";
            return;
        }
        value = {numbers[0], numbers[1]};
    }
};

namespace advanced_platformer
{
    struct SpriteJson
    {
        glm::vec2 position{};
        glm::vec2 size{};
        std::optional<SpriteAnchor> anchor;
    };

    Sprite spriteFrom(const SpriteJson& json);

    std::string loadContentText(const std::filesystem::path& path);

    [[noreturn]] void failContentRead(
        std::string_view text,
        std::string_view sourceName,
        const glz::error_ctx& error);

    template <class T> T readContent(std::string_view text, std::string_view sourceName)
    {
        T result{};
        if (const auto error = glz::read<glz::opts{.error_on_missing_keys = true}>(result, text))
        {
            failContentRead(text, sourceName, error);
        }
        return result;
    }
}
