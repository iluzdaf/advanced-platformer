#include "content_glaze.hpp"

#include "content_diagnostics.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

#include <glaze/glaze.hpp>

#include "advanced_platformer/render/sprite.hpp"

namespace advanced_platformer
{
    namespace
    {
        // Where the key or value an error points at begins. Glaze may point just past a
        // string's opening quote, or partway through a number it could not read.
        std::size_t tokenStart(std::string_view text, std::size_t offset)
        {
            if (offset >= text.size() || text[offset] == '"')
            {
                return offset;
            }
            if (offset > 0 && text[offset - 1] == '"')
            {
                return offset - 1;
            }
            constexpr std::string_view NumberCharacters = "0123456789+-.eE";
            std::size_t start = offset;
            while (start > 0 && NumberCharacters.contains(text[start - 1]))
            {
                --start;
            }
            return start;
        }

        // The key or value beginning here: a quoted string without its quotes, or the text up
        // to the next delimiter.
        std::string_view tokenAt(std::string_view text, std::size_t start)
        {
            const std::string_view rest = text.substr(start);
            if (rest.starts_with('"'))
            {
                const std::string_view quoted = rest.substr(1);
                return quoted.substr(0, quoted.find('"'));
            }
            return rest.substr(0, rest.find_first_of(",}] \t\r\n"));
        }

        // Glaze's name for the error, in words: "unexpected end", "expected comma".
        std::string codeName(const glz::error_ctx& error)
        {
            std::string name(glz::meta<glz::error_code>::keys[static_cast<std::size_t>(error.ec)]);
            std::ranges::replace(name, '_', ' ');
            return name;
        }

        std::string describe(const glz::error_ctx& error, std::string_view token)
        {
            switch (error.ec)
            {
            case glz::error_code::unknown_key:
                return std::format("unknown field '{}'", token);
            case glz::error_code::missing_key:
                return std::format("missing '{}'", error.custom_error_message);
            case glz::error_code::unexpected_enum:
                return std::format("unknown value '{}'; {}", token, error.custom_error_message);
            case glz::error_code::expected_quote:
                return std::format("expected text, found '{}'", token);
            case glz::error_code::parse_number_failure:
                return std::format("invalid number '{}'", token);
            case glz::error_code::expected_brace:
                return std::format("expected an object, found '{}'", token);
            case glz::error_code::no_matching_variant_type:
                return std::format("unexpected '{}', which is not a form this field takes", token);
            case glz::error_code::expected_bracket:
                return std::format("expected a list, found '{}'", token);
            default:
                break;
            }
            if (!error.custom_error_message.empty())
            {
                return std::string(error.custom_error_message);
            }
            return codeName(error);
        }
    }

    Sprite spriteFrom(const SpriteJson& json)
    {
        Sprite sprite;
        sprite.region = {json.position, json.size};
        sprite.size = json.displaySize.value_or(json.size);
        sprite.anchor = json.anchor.value_or(SpriteAnchor::BodyFeet);
        return sprite;
    }

    std::string loadContentText(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file)
        {
            throw std::invalid_argument(
                std::format(
                    "Could not open content file '{}'", std::filesystem::absolute(path).string()));
        }
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    void failContentRead(
        std::string_view text,
        std::string_view sourceName,
        const glz::error_ctx& readError)
    {
        // Text that is not JSON at all is a syntax error, reported where the syntax breaks,
        // rather than a shape error at wherever the read gave up.
        const glz::error_ctx syntaxError = glz::validate_json(text);
        const bool isSyntaxError = bool(syntaxError);
        const glz::error_ctx& error = isSyntaxError ? syntaxError : readError;
        const std::size_t offset = tokenStart(text, std::min(error.count, text.size()));
        const std::string_view before = text.substr(0, offset);
        const std::size_t line = 1 + static_cast<std::size_t>(std::ranges::count(before, '\n'));
        const std::size_t lineStart = before.rfind('\n');
        const std::size_t column =
            lineStart == std::string_view::npos ? offset + 1 : offset - lineStart;
        failJson(
            sourceName,
            std::format("line {}, column {}", line, column),
            isSyntaxError ? std::format("invalid JSON: {}", codeName(error))
                          : describe(error, tokenAt(text, offset)));
    }
}
