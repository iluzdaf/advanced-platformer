#pragma once

#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <glaze/glaze.hpp>

namespace tests
{
    // An editable JSON document, for tests that start from valid content and break one
    // part of it.
    using Json = glz::generic;

    inline Json parseJson(std::string_view text)
    {
        Json document;
        if (const auto error = glz::read_json(document, text))
        {
            throw std::invalid_argument(glz::format_error(error, text));
        }
        return document;
    }

    // The document as text, to hand to a parser.
    inline std::string dumpJson(const Json& document)
    {
        return document.dump().value();
    }

    inline void eraseKey(Json& object, std::string_view key)
    {
        object.get_object().erase(key);
    }

    // A list of numbers, such as [0, 24].
    inline Json numbers(std::initializer_list<double> values)
    {
        return Json::array_t(values.begin(), values.end());
    }

    // A list of any values, such as ["a", 1].
    inline Json list(std::initializer_list<Json> values)
    {
        return Json::array_t(values.begin(), values.end());
    }

    // An object, such as {"item": "key", "quantity": 1}.
    inline Json object(std::initializer_list<std::pair<const char*, Json>> members)
    {
        Json::object_t result;
        for (const auto& [key, value] : members)
        {
            result.emplace(key, value);
        }
        return result;
    }

    inline Json emptyArray()
    {
        return Json::array_t{};
    }

    inline Json emptyObject()
    {
        return Json::object_t{};
    }
}
