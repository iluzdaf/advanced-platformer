#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glaze/glaze.hpp>
#include <glm/vec2.hpp>

#include "advanced_platformer/render/sprite.hpp"

namespace advanced_platformer
{
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

}

template <> struct glz::meta<advanced_platformer::SpriteAnchor>
{
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr std::array keys{"feet", "center"};
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr std::array value{
        advanced_platformer::SpriteAnchor::BodyFeet,
        advanced_platformer::SpriteAnchor::BodyCenter};
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
