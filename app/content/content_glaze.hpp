#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <glaze/glaze.hpp>
#include <glm/vec2.hpp>
#include "advanced_platformer/render/sprite.hpp"

// Content files read with Glaze through structs that mirror them: their member names are the
// file's keys. These declarations teach Glaze the shared shapes those structs use.

namespace advanced_platformer
{
    // The names a content file writes for an enum's values, in the order an error lists them.
    // Specialize it with a Names array of {name, value} pairs, and read the enum with
    // NamedEnumReader:
    //
    //   template <> struct ContentNames<ItemEffect>
    //   {
    //       static constexpr std::array Names{
    //           std::pair{std::string_view{"none"}, ItemEffect::None}, ...};
    //   };
    //   template <> struct glz::from<glz::JSON, ItemEffect> : NamedEnumReader<ItemEffect> {};
    template <class Enum> struct ContentNames;

    // "a", "a or b", "a, b or c".
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

    // Reads an enum from one of its ContentNames. An unknown name is an error that lists the
    // names it could have been.
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

namespace advanced_platformer
{
    // A struct read from an object that may leave any member out, which then keeps its C++
    // default. Unknown keys are still errors. The struct's member names are the keys.
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

// A vector is written as [x, y], with exactly two numbers.
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
    // key or value found there, or "invalid JSON: ..." when the text is not JSON at all.
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
