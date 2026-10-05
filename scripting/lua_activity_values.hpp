#pragma once

#include <initializer_list>
#include <string_view>

#include "advanced_platformer/npc/npc_activity_scripts.hpp"

// NOLINTBEGIN(misc-include-cleaner)
#include <sol/sol.hpp>
#include <glm/vec2.hpp>

namespace advanced_platformer
{
    float number(const sol::object& object, std::string_view field);

    sol::object luaVector(sol::state& lua, glm::vec2 value);

    void rejectUnknownFields(
        const sol::table& table,
        std::initializer_list<std::string_view> allowed,
        std::string_view subject);

    sol::table luaSnapshot(sol::state& lua, const NpcActivitySnapshot& snapshot);

    NpcActivityCommand commandFrom(const sol::object& object);
}

// NOLINTEND(misc-include-cleaner)
