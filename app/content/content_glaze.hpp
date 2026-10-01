#pragma once
#include <optional>
#include <string_view>
#include <vector>
#include <glaze/glaze.hpp>
#include <glm/vec2.hpp>
#include "advanced_platformer/render/sprite.hpp"

// Content files read with Glaze through structs that mirror them: their member names are the
// file's keys. These declarations teach Glaze the shared shapes those structs use.

// A sprite's anchor is written as "feet" or "center".
template <> struct glz::meta<advanced_platformer::SpriteAnchor>
{
    using enum advanced_platformer::SpriteAnchor;
    // NOLINTNEXTLINE(readability-identifier-naming): Glaze looks this member up by name.
    static constexpr auto value = glz::enumerate("feet", BodyFeet, "center", BodyCenter);
};

// A vector is written as [x, y], with exactly two numbers.
template <> struct glz::from<glz::JSON, glm::vec2>
{
    template <auto Options> static void op(glm::vec2& value, auto&& context, auto&& it, auto&& end)
    {
        std::vector<float> numbers;
        parse<JSON>::op<Options>(numbers, context, it, end);
        if (bool(context.error))
        {
            return;
        }
        if (numbers.size() != 2)
        {
            context.error = error_code::syntax_error;
            context.custom_error_message = "expected two numbers, [x, y]";
            return;
        }
        value = {numbers[0], numbers[1]};
    }
};

namespace advanced_platformer
{
    // A sprite as a content file writes it: a region of the atlas, with an optional display
    // size, which defaults to the region's, and an optional anchor.
    struct SpriteJson
    {
        glm::vec2 position{};
        glm::vec2 size{};
        std::optional<glm::vec2> displaySize;
        std::optional<SpriteAnchor> anchor;
    };

    Sprite spriteFrom(const SpriteJson& json);

    // Reports a Glaze read error as "source: line L, column C: what was wrong", naming the
    // key or value found there.
    [[noreturn]] void failContentRead(
        std::string_view text,
        std::string_view sourceName,
        const glz::error_ctx& error);

    // Reads a content file into the struct that mirrors it. Every key must be one the struct
    // has, and every member must be present unless it is a std::optional. This checks shape
    // only; the catalog's validator checks the values.
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
