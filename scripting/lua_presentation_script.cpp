#include "lua_presentation_script.hpp"

#include "lua_activity_values.hpp"
#include "lua_sandbox.hpp"

#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// NOLINTBEGIN(misc-include-cleaner)
#include <sol/sol.hpp>

#include "advanced_platformer/render/presentation_scripts.hpp"
#include "advanced_platformer/world/world.hpp"

namespace advanced_platformer
{
    namespace
    {
        constexpr std::string_view ScriptName = "presentation";

        struct LoadedScript
        {
            std::string source;
            sol::environment environment;
            sol::table hooks;
        };

        std::string resultError(sol::protected_function_result& result)
        {
            const sol::error error = result;
            return error.what();
        }

        [[noreturn]] void fail(std::string_view sourceName, std::string_view reason)
        {
            throw std::invalid_argument(
                std::format("Lua presentation script in {}{}", sourceName, reason));
        }

        std::string_view kindName(WorldEventKind kind)
        {
            switch (kind)
            {
            case WorldEventKind::Landing:
                return "landing";
            case WorldEventKind::Shot:
                return "shot";
            case WorldEventKind::Knockback:
                return "knockback";
            }
            return "unknown";
        }

        float positiveNumber(const sol::object& object, std::string_view field)
        {
            const float value = number(object, field);
            if (!(value > 0.0F))
            {
                throw std::invalid_argument(std::format("{} must be greater than zero", field));
            }
            return value;
        }

        PresentationEffects effectsFrom(const sol::object& object)
        {
            PresentationEffects effects;
            if (!object.valid() || object.get_type() == sol::type::lua_nil)
            {
                return effects;
            }
            if (!object.is<sol::table>())
            {
                throw std::invalid_argument("presentation effects must be a table or nil");
            }
            const sol::table table = object.as<sol::table>();
            rejectUnknownFields(table, {"shake"}, "presentation effects");
            const sol::object shake = table.get<sol::object>("shake");
            if (shake.valid() && shake.get_type() != sol::type::lua_nil)
            {
                if (!shake.is<sol::table>())
                {
                    throw std::invalid_argument("shake must be a table");
                }
                const sol::table shakeTable = shake.as<sol::table>();
                rejectUnknownFields(shakeTable, {"duration", "magnitude"}, "shake");
                effects.shake = CameraShakeEffect{
                    positiveNumber(shakeTable.get<sol::object>("duration"), "shake.duration"),
                    positiveNumber(shakeTable.get<sol::object>("magnitude"), "shake.magnitude")};
            }
            return effects;
        }

        std::string_view presentationHookName(WorldEventKind kind)
        {
            switch (kind)
            {
            case WorldEventKind::Landing:
                return "onLanding";
            case WorldEventKind::Shot:
                return "onShot";
            case WorldEventKind::Knockback:
                return "onKnockback";
            }
            return "";
        }
    }

    struct LuaPresentationScript::Implementation
    {
        sol::state lua;
        std::optional<LoadedScript> script;
        std::vector<LuaScriptDiagnostic> reported;
        std::string callingHook;
        std::string callingSource;

        Implementation()
        {
            openSandbox(lua);
            lua.set_function(
                "print",
                [this](sol::variadic_args arguments)
                {
                    const sol::protected_function tostring = lua["tostring"];
                    std::string text;
                    for (const sol::object argument : arguments)
                    {
                        if (!text.empty())
                        {
                            text.push_back('\t');
                        }
                        text.append(tostring(argument).get<std::string>());
                    }
                    record(std::move(text), LuaScriptDiagnosticKind::Print);
                });
        }

        void record(std::string message, LuaScriptDiagnosticKind kind)
        {
            reported.push_back(
                {callingSource,
                 std::string(ScriptName),
                 {},
                 callingHook,
                 std::nullopt,
                 std::move(message),
                 kind});
        }
    };

    LuaPresentationScript::LuaPresentationScript()
        : implementation(std::make_unique<Implementation>())
    {
    }

    LuaPresentationScript::~LuaPresentationScript() = default;
    LuaPresentationScript::LuaPresentationScript(LuaPresentationScript&& other) noexcept = default;
    LuaPresentationScript& LuaPresentationScript::operator=(
        LuaPresentationScript&& other) noexcept = default;

    void LuaPresentationScript::loadScript(const std::filesystem::path& path)
    {
        std::ifstream input(path);
        if (!input)
        {
            throw std::invalid_argument(
                std::format(
                    "Cannot read Lua script '{}'", std::filesystem::absolute(path).string()));
        }
        std::ostringstream source;
        source << input.rdbuf();
        loadScriptText(source.str(), path.string());
    }

    void LuaPresentationScript::loadScriptText(std::string_view source, std::string sourceName)
    {
        if (sourceName.empty())
        {
            throw std::invalid_argument("A Lua script needs a source name");
        }

        sol::environment fresh(implementation->lua, sol::create, implementation->lua.globals());
        sol::protected_function_result result;
        {
            implementation->callingSource = sourceName;
            implementation->callingHook = "load";
            const InstructionBudget budget(implementation->lua.lua_state());
            result = implementation->lua.safe_script(
                source, fresh, sol::script_pass_on_error, sourceName);
            implementation->callingHook.clear();
        }
        if (!result.valid())
        {
            fail(sourceName, ": " + resultError(result));
        }

        const sol::object returned = result;
        if (!returned.is<sol::table>())
        {
            fail(sourceName, " must return a table of hooks");
        }
        const sol::table hooks = returned.as<sol::table>();
        rejectUnknownFields(
            hooks, {"onLanding", "onShot", "onKnockback"}, "a Lua presentation script");
        for (const auto& [nameObject, hook] : hooks)
        {
            if (!hook.is<sol::function>())
            {
                fail(
                    sourceName,
                    std::format(
                        " has a {} value that is not a function", nameObject.as<std::string>()));
            }
        }

        implementation->script = LoadedScript{std::move(sourceName), std::move(fresh), hooks};
        implementation->callingSource = implementation->script->source;
    }

    bool LuaPresentationScript::loaded() const
    {
        return implementation->script.has_value();
    }

    PresentationEffects LuaPresentationScript::onEvent(const WorldEvent& event, bool player)
    {
        if (!implementation->script.has_value())
        {
            return {};
        }
        const std::string hookName(presentationHookName(event.kind));
        const sol::object hookObject = implementation->script->hooks.get<sol::object>(hookName);
        if (!hookObject.valid() || hookObject.get_type() == sol::type::lua_nil)
        {
            return {};
        }

        sol::state& lua = implementation->lua;
        const sol::table luaEvent = lua.create_table_with(
            "kind",
            std::string(kindName(event.kind)),
            "actor",
            std::string(player ? "player" : "npc"),
            "feet",
            luaVector(lua, event.feet),
            "velocity",
            luaVector(lua, event.velocity));

        sol::protected_function hook = hookObject.as<sol::protected_function>();
        sol::protected_function_result result;
        {
            implementation->callingHook = hookName;
            const InstructionBudget budget(lua.lua_state());
            result = hook(luaEvent);
        }
        if (!result.valid())
        {
            implementation->record(resultError(result), LuaScriptDiagnosticKind::Error);
            implementation->callingHook.clear();
            return {};
        }

        try
        {
            const sol::object returned = result;
            const PresentationEffects effects = effectsFrom(returned);
            implementation->callingHook.clear();
            return effects;
        }
        catch (const std::invalid_argument& error)
        {
            implementation->record(error.what(), LuaScriptDiagnosticKind::Error);
            implementation->callingHook.clear();
            return {};
        }
    }

    const std::vector<LuaScriptDiagnostic>& LuaPresentationScript::diagnostics() const
    {
        return implementation->reported;
    }

    std::vector<LuaScriptDiagnostic> LuaPresentationScript::takeDiagnostics()
    {
        return std::exchange(implementation->reported, {});
    }
}

// NOLINTEND(misc-include-cleaner)
