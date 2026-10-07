#include "lua_activity_values.hpp"

#include <cmath>
#include <format>
#include <initializer_list>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <glm/vec2.hpp>

#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/navigation/navigation_path.hpp"
#include "advanced_platformer/npc/npc_activity_scripts.hpp"

// NOLINTBEGIN(misc-include-cleaner)
#include <sol/sol.hpp>

namespace advanced_platformer
{
    float number(const sol::object& object, std::string_view field)
    {
        if (object.get_type() != sol::type::number)
        {
            throw std::invalid_argument(std::format("{} must be a number", field));
        }
        const double value = object.as<double>();
        if (!std::isfinite(value) ||
            value < -static_cast<double>(std::numeric_limits<float>::max()) ||
            value > static_cast<double>(std::numeric_limits<float>::max()))
        {
            throw std::invalid_argument(std::format("{} must be finite", field));
        }
        return static_cast<float>(value);
    }

    sol::object luaVector(sol::state& lua, glm::vec2 value)
    {
        return sol::make_object(lua, value);
    }

    namespace
    {

        bool boolean(const sol::object& object, std::string_view field)
        {
            if (object.get_type() != sol::type::boolean)
            {
                throw std::invalid_argument(std::format("{} must be true or false", field));
            }
            return object.as<bool>();
        }

        ClimbGrip climbGrip(const sol::object& object, std::string_view field)
        {
            if (object.get_type() == sol::type::string)
            {
                const std::string name = object.as<std::string>();
                if (name == "keep")
                {
                    return ClimbGrip::Keep;
                }
                if (name == "hold")
                {
                    return ClimbGrip::Hold;
                }
                if (name == "release")
                {
                    return ClimbGrip::Release;
                }
            }
            throw std::invalid_argument(
                std::format("{} must be \"keep\", \"hold\" or \"release\"", field));
        }

        glm::vec2 vector(const sol::object& object, std::string_view field)
        {
            if (object.get_type() == sol::type::userdata && object.is<glm::vec2>())
            {
                const glm::vec2 value = object.as<glm::vec2>();
                requireFinite(value, std::string(field).c_str());
                return value;
            }
            if (!object.is<sol::table>())
            {
                throw std::invalid_argument(
                    std::format("{} must be a vec2 or an {{x, y}} table", field));
            }
            const sol::table table = object.as<sol::table>();
            rejectUnknownFields(table, {"x", "y"}, field);
            return {
                number(table.get<sol::object>("x"), std::format("{}.x", field)),
                number(table.get<sol::object>("y"), std::format("{}.y", field))};
        }

    }

    void rejectUnknownFields(
        const sol::table& table,
        std::initializer_list<std::string_view> allowed,
        std::string_view subject)
    {
        for (const auto& [keyObject, value] : table)
        {
            (void)value;
            if (!keyObject.is<std::string>())
            {
                throw std::invalid_argument(
                    std::format("{} has a field whose name is not text", subject));
            }
            const std::string key = keyObject.as<std::string>();
            bool known = false;
            for (std::string_view name : allowed)
            {
                known = known || name == key;
            }
            if (!known)
            {
                throw std::invalid_argument(std::format("{} has unknown field '{}'", subject, key));
            }
        }
    }

    namespace
    {
        const char* routeStatusName(NavigationPathStatus status)
        {
            switch (status)
            {
            case NavigationPathStatus::Found:
                return "found";
            case NavigationPathStatus::Unreachable:
                return "unreachable";
            case NavigationPathStatus::Deferred:
                return "deferred";
            }
            return "unknown";
        }
    }

    sol::table luaSnapshot(sol::state& lua, const NpcActivitySnapshot& snapshot)
    {
        sol::table result = lua.create_table();
        const auto optionalVector = [&lua](const std::optional<glm::vec2>& value)
        {
            return value.has_value() ? sol::make_object(lua, luaVector(lua, *value))
                                     : sol::make_object(lua, sol::lua_nil);
        };
        result["feet"] = luaVector(lua, snapshot.feet);
        result["center"] = luaVector(lua, snapshot.center);
        result["targetFeet"] = optionalVector(snapshot.targetFeet);
        result["lastKnownTargetFeet"] = luaVector(lua, snapshot.lastKnownTargetFeet);
        result["targetCenter"] = optionalVector(snapshot.targetCenter);
        if (snapshot.patrol.has_value())
        {
            result["patrol"] = lua.create_table_with(
                "firstFeet",
                luaVector(lua, snapshot.patrol->firstFeet),
                "secondFeet",
                luaVector(lua, snapshot.patrol->secondFeet),
                "headingToSecond",
                snapshot.patrol->headingToSecond);
        }
        else
        {
            result["patrol"] = sol::lua_nil;
        }
        if (snapshot.footing.has_value())
        {
            result["footing"] = lua.create_table_with(
                "left", snapshot.footing->left, "right", snapshot.footing->right);
        }
        else
        {
            result["footing"] = sol::lua_nil;
        }
        result["stateElapsed"] = snapshot.facts.stateElapsed;
        result["routeStatus"] = snapshot.routeStatus.has_value()
                                    ? sol::make_object(lua, routeStatusName(*snapshot.routeStatus))
                                    : sol::make_object(lua, sol::lua_nil);
        result["routeComplete"] = snapshot.routeComplete;
        result["exitFeet"] = optionalVector(snapshot.exitFeet);
        sol::table pickups = lua.create_table();
        for (const glm::vec2 feet : snapshot.pickups)
        {
            pickups.add(luaVector(lua, feet));
        }
        result["pickups"] = pickups;

        sol::table facts = lua.create_table();
        facts["targetKnown"] = snapshot.facts.targetKnown;
        facts["targetVisible"] = snapshot.facts.targetVisible;
        facts["targetInPrimaryRange"] = snapshot.facts.targetInPrimaryRange;
        facts["primaryReady"] = snapshot.facts.primaryReady;
        facts["primaryActive"] = snapshot.facts.primaryActive;
        facts["targetInSecondaryRange"] = snapshot.facts.targetInSecondaryRange;
        facts["secondaryReady"] = snapshot.facts.secondaryReady;
        facts["secondaryActive"] = snapshot.facts.secondaryActive;
        facts["targetWithinStandoffDistance"] = snapshot.facts.targetWithinStandoffDistance;
        facts["heardLanding"] = snapshot.facts.heardLanding;
        facts["targetOnSameSurface"] = snapshot.facts.targetOnSameSurface;
        facts["targetWithinNoticeDistance"] = snapshot.facts.targetWithinNoticeDistance;
        facts["movementBlocked"] = snapshot.facts.movementBlocked;
        facts["hasPatrol"] = snapshot.facts.hasPatrol;
        facts["searches"] = snapshot.facts.searches;
        facts["searchTimeUp"] = snapshot.facts.searchTimeUp;
        result["facts"] = facts;
        return result;
    }

    NpcActivityCommand commandFrom(const sol::object& object)
    {
        NpcActivityCommand command;
        if (!object.valid() || object.get_type() == sol::type::lua_nil)
        {
            return command;
        }
        if (!object.is<sol::table>())
        {
            throw std::invalid_argument("an activity update must return a command table or nil");
        }

        const sol::table table = object.as<sol::table>();
        rejectUnknownFields(
            table,
            {"direction",
             "aimDirection",
             "jumpPressed",
             "jumpHeld",
             "primaryAttackPressed",
             "secondaryAttackPressed",
             "climbGrip",
             "avoidLedges",
             "routeTo",
             "aimAt",
             "clearRoute",
             "turnPatrol"},
            "an activity command");

        const auto readVector = [&](std::string_view name, glm::vec2& destination)
        {
            const sol::object value = table.get<sol::object>(name);
            if (value.valid() && value.get_type() != sol::type::lua_nil)
            {
                destination = vector(value, std::format("command.{}", name));
            }
        };
        const auto readOptionalVector =
            [&](std::string_view name, std::optional<glm::vec2>& destination)
        {
            const sol::object value = table.get<sol::object>(name);
            if (value.valid() && value.get_type() != sol::type::lua_nil)
            {
                destination = vector(value, std::format("command.{}", name));
            }
        };
        const auto readClimbGrip = [&](std::string_view name, ClimbGrip& destination)
        {
            const sol::object value = table.get<sol::object>(name);
            if (value.valid() && value.get_type() != sol::type::lua_nil)
            {
                destination = climbGrip(value, std::format("command.{}", name));
            }
        };
        const auto readBoolean = [&](std::string_view name, bool& destination)
        {
            const sol::object value = table.get<sol::object>(name);
            if (value.valid() && value.get_type() != sol::type::lua_nil)
            {
                destination = boolean(value, std::format("command.{}", name));
            }
        };

        readVector("direction", command.intentions.direction);
        readVector("aimDirection", command.intentions.aimDirection);
        readBoolean("jumpPressed", command.intentions.jumpPressed);
        readBoolean("jumpHeld", command.intentions.jumpHeld);
        readBoolean("primaryAttackPressed", command.intentions.primaryAttackPressed);
        readBoolean("secondaryAttackPressed", command.intentions.secondaryAttackPressed);
        readClimbGrip("climbGrip", command.intentions.climbGrip);
        readBoolean("avoidLedges", command.intentions.avoidLedges);
        readOptionalVector("routeTo", command.routeTo);
        readOptionalVector("aimAt", command.aimAt);
        readBoolean("clearRoute", command.clearRoute);
        readBoolean("turnPatrol", command.turnPatrol);
        return command;
    }
}

// NOLINTEND(misc-include-cleaner)
