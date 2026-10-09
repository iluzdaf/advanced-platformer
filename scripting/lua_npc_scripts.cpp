#include "lua_npc_scripts.hpp"

#include "lua_activity_values.hpp"
#include "lua_sandbox.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

// NOLINTBEGIN(misc-include-cleaner)
#include <sol/sol.hpp>

#include "advanced_platformer/math/validation.hpp"
#include "advanced_platformer/npc/npc_fact_rows.hpp"

namespace advanced_platformer
{
    namespace
    {
        struct ActivityOwner
        {
            std::uint32_t actor = 0;
            std::string script;
            std::string activity;

            auto operator<=>(const ActivityOwner&) const = default;
        };

        struct ScriptFact
        {
            std::string name;
            sol::protected_function answer;
        };

        struct LoadedScript
        {
            std::string source;
            sol::environment environment;
            sol::table activities;
            std::vector<ScriptFact> facts;
        };

        struct MemoryOwner
        {
            std::uint32_t actor = 0;
            std::string script;

            auto operator<=>(const MemoryOwner&) const = default;
        };

        void requireValidCall(ActorId actor, const NpcActivity& activity)
        {
            if (actor.value == 0)
            {
                throw std::invalid_argument("NPC script calls require a valid actor ID");
            }
            if (activity.script.empty() || activity.activity.empty())
            {
                throw std::invalid_argument("NPC script calls require a script and activity name");
            }
        }

        std::string resultError(sol::protected_function_result& result)
        {
            const sol::error error = result;
            return error.what();
        }

        std::string scriptDescription(std::string_view script, std::string_view source)
        {
            std::string description = "Lua script '";
            description.append(script);
            description.append("' in ");
            description.append(source);
            return description;
        }

        std::string activityDescription(
            std::string_view script,
            std::string_view activity,
            std::string_view source)
        {
            std::string description = "Lua activity '";
            description.append(script);
            description.push_back('.');
            description.append(activity);
            description.append("' in ");
            description.append(source);
            return description;
        }

        [[noreturn]] void fail(std::string description, std::string_view reason)
        {
            description.append(reason);
            throw std::invalid_argument(description);
        }
    }

    struct LuaNpcScripts::Implementation
    {
        struct Call
        {
            std::string source;
            std::string script;
            std::string activity;
            std::string hook;
            std::optional<ActorId> actor;
        };

        class CallScope
        {
        public:
            CallScope(Implementation& implementation, Call call)
                : implementation(implementation)
            {
                implementation.calling = std::move(call);
            }

            ~CallScope()
            {
                implementation.calling.reset();
            }

            CallScope(const CallScope&) = delete;
            CallScope& operator=(const CallScope&) = delete;

        private:
            Implementation& implementation;
        };

        sol::state lua;
        std::map<std::string, LoadedScript> scripts;
        std::map<ActivityOwner, sol::table> selves;
        std::map<MemoryOwner, sol::table> memories;
        std::vector<LuaScriptDiagnostic> reported;
        std::optional<Call> calling;

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
                    recordPrint(std::move(text));
                });
        }

        void recordPrint(std::string text)
        {
            const Call call = calling.value_or(Call{});
            reported.push_back(
                {call.source,
                 call.script,
                 call.activity,
                 call.hook,
                 call.actor,
                 std::move(text),
                 LuaScriptDiagnosticKind::Print});
        }

        Call callTo(ActorId actor, const NpcActivity& activity, std::string hook) const
        {
            const LoadedScript* script = scriptNamed(activity.script);
            return {
                script == nullptr ? std::string{} : script->source,
                activity.script,
                activity.activity,
                std::move(hook),
                actor};
        }

        LoadedScript* scriptNamed(std::string_view name)
        {
            const auto found = scripts.find(std::string(name));
            return found == scripts.end() ? nullptr : &found->second;
        }

        const LoadedScript* scriptNamed(std::string_view name) const
        {
            const auto found = scripts.find(std::string(name));
            return found == scripts.end() ? nullptr : &found->second;
        }

        std::optional<sol::table> activityTable(const NpcActivity& activity) const
        {
            const LoadedScript* script = scriptNamed(activity.script);
            if (script == nullptr)
            {
                return std::nullopt;
            }
            const sol::object found = script->activities.get<sol::object>(activity.activity);
            if (!found.is<sol::table>())
            {
                return std::nullopt;
            }
            return found.as<sol::table>();
        }

        void report(
            ActorId actor,
            const NpcActivity& activity,
            std::string hook,
            std::string message)
        {
            const LoadedScript* script = scriptNamed(activity.script);
            reported.push_back(
                {script == nullptr ? std::string{} : script->source,
                 activity.script,
                 activity.activity,
                 std::move(hook),
                 actor,
                 std::move(message)});
        }

        sol::table memoryOf(ActorId actor, const std::string& script)
        {
            const MemoryOwner owner{actor.value, script};
            const auto found = memories.find(owner);
            if (found != memories.end())
            {
                return found->second;
            }
            sol::table memory = lua.create_table();
            memories.emplace(owner, memory);
            return memory;
        }

        void discardScriptState(std::string_view script)
        {
            for (auto entry = memories.begin(); entry != memories.end();)
            {
                if (entry->first.script == script)
                {
                    entry = memories.erase(entry);
                }
                else
                {
                    ++entry;
                }
            }
            for (auto entry = selves.begin(); entry != selves.end();)
            {
                if (entry->first.script == script)
                {
                    entry = selves.erase(entry);
                }
                else
                {
                    ++entry;
                }
            }
        }
    };

    LuaNpcScripts::LuaNpcScripts()
        : implementation(std::make_unique<Implementation>())
    {
    }

    LuaNpcScripts::~LuaNpcScripts() = default;
    LuaNpcScripts::LuaNpcScripts(LuaNpcScripts&& other) noexcept = default;
    LuaNpcScripts& LuaNpcScripts::operator=(LuaNpcScripts&& other) noexcept = default;

    void LuaNpcScripts::loadScript(const std::string& script, const std::filesystem::path& path)
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
        loadScriptText(script, source.str(), path.string());
    }

    namespace
    {
        std::vector<ScriptFact> scriptFacts(
            const sol::table& root,
            std::string_view script,
            std::string_view sourceName)
        {
            std::vector<ScriptFact> facts;
            const sol::object factsObject = root.get<sol::object>("facts");
            if (!factsObject.valid() || factsObject.get_type() == sol::type::lua_nil)
            {
                return facts;
            }
            if (!factsObject.is<sol::table>())
            {
                fail(
                    scriptDescription(script, sourceName),
                    " has a facts value that is not a table");
            }
            for (const auto& [nameObject, answerObject] : factsObject.as<sol::table>())
            {
                if (!nameObject.is<std::string>() || nameObject.as<std::string>().empty())
                {
                    fail(scriptDescription(script, sourceName), " needs named facts");
                }
                const std::string name = nameObject.as<std::string>();
                if (!answerObject.is<sol::function>())
                {
                    fail(
                        scriptDescription(script, sourceName),
                        std::format(" has a fact '{}' that is not a function", name));
                }
                if (npcFactRow(name) != nullptr)
                {
                    fail(
                        scriptDescription(script, sourceName),
                        std::format(" has a fact '{}' that the engine already answers", name));
                }
                facts.push_back({name, answerObject.as<sol::protected_function>()});
            }
            std::ranges::sort(facts, {}, &ScriptFact::name);
            return facts;
        }
    }

    void LuaNpcScripts::loadScriptText(
        const std::string& script,
        std::string_view source,
        std::string sourceName)
    {
        if (script.empty())
        {
            throw std::invalid_argument("A Lua script needs a name");
        }
        if (sourceName.empty())
        {
            throw std::invalid_argument("A Lua script needs a source name");
        }

        sol::environment fresh(implementation->lua, sol::create, implementation->lua.globals());
        sol::protected_function_result result;
        {
            const Implementation::CallScope call(
                *implementation, {sourceName, script, {}, "load", std::nullopt});
            const InstructionBudget budget(implementation->lua.lua_state());
            result = implementation->lua.safe_script(
                source, fresh, sol::script_pass_on_error, sourceName);
        }
        if (!result.valid())
        {
            const std::string error = resultError(result);
            std::string reason = ": ";
            reason.append(error);
            fail(scriptDescription(script, sourceName), reason);
        }

        const sol::object returned = result;
        if (!returned.is<sol::table>())
        {
            fail(scriptDescription(script, sourceName), " must return a table");
        }
        const sol::table root = returned.as<sol::table>();
        rejectUnknownFields(root, {"activities", "facts"}, "a Lua script");
        const sol::object activitiesObject = root.get<sol::object>("activities");
        if (!activitiesObject.is<sol::table>())
        {
            fail(scriptDescription(script, sourceName), " needs an activities table");
        }

        const sol::table activities = activitiesObject.as<sol::table>();
        std::set<std::string> names;
        for (const auto& [nameObject, activityObject] : activities)
        {
            if (!nameObject.is<std::string>())
            {
                fail(
                    scriptDescription(script, sourceName),
                    " has an activity whose name is not text");
            }
            const std::string name = nameObject.as<std::string>();
            if (name.empty() || !activityObject.is<sol::table>())
            {
                fail(scriptDescription(script, sourceName), " needs named activity tables");
            }
            names.insert(name);
            const sol::table activity = activityObject.as<sol::table>();
            rejectUnknownFields(
                activity,
                {"description", "enter", "update", "exit"},
                std::format("Lua activity '{}'", name));
            if (const sol::object description = activity.get<sol::object>("description");
                description.valid() && description.get_type() != sol::type::lua_nil &&
                description.get_type() != sol::type::string)
            {
                fail(
                    activityDescription(script, name, sourceName),
                    " has a description that is not text");
            }
            if (!activity.get<sol::object>("update").is<sol::function>())
            {
                fail(activityDescription(script, name, sourceName), " needs an update function");
            }
            for (std::string_view optional : {std::string_view{"enter"}, std::string_view{"exit"}})
            {
                const sol::object hook = activity.get<sol::object>(optional);
                if (hook.valid() && hook.get_type() != sol::type::lua_nil &&
                    !hook.is<sol::function>())
                {
                    std::string reason = " has a ";
                    reason.append(optional);
                    reason.append(" value that is not a function");
                    fail(activityDescription(script, name, sourceName), reason);
                }
            }
        }
        if (names.empty())
        {
            fail(scriptDescription(script, sourceName), " needs at least one activity");
        }

        std::vector<ScriptFact> facts = scriptFacts(root, script, sourceName);
        implementation->discardScriptState(script);
        implementation->scripts.insert_or_assign(
            script,
            LoadedScript{std::move(sourceName), std::move(fresh), activities, std::move(facts)});
    }

    bool LuaNpcScripts::hasActivity(const NpcActivity& activity) const
    {
        return implementation->activityTable(activity).has_value();
    }

    std::vector<std::string> LuaNpcScripts::factNames(const std::string& script) const
    {
        std::vector<std::string> names;
        if (const LoadedScript* loaded = implementation->scriptNamed(script); loaded != nullptr)
        {
            for (const ScriptFact& fact : loaded->facts)
            {
                names.push_back(fact.name);
            }
        }
        return names;
    }

    bool LuaNpcScripts::hasFacts(const std::string& script) const
    {
        const LoadedScript* loaded = implementation->scriptNamed(script);
        return loaded != nullptr && !loaded->facts.empty();
    }

    std::map<std::string, bool> LuaNpcScripts::facts(
        ActorId actor,
        const std::string& script,
        const NpcActivitySnapshot& snapshot,
        float deltaTime)
    {
        requireSeconds(deltaTime, "NPC script fact time step");
        std::map<std::string, bool> answers;
        const LoadedScript* loaded = implementation->scriptNamed(script);
        if (loaded == nullptr || loaded->facts.empty())
        {
            return answers;
        }
        requireValidCall(actor, {script, loaded->facts.front().name});
        const sol::table memory = implementation->memoryOf(actor, script);
        const sol::table view = luaSnapshot(implementation->lua, snapshot);
        for (const ScriptFact& fact : loaded->facts)
        {
            const NpcActivity asked{script, fact.name};
            sol::protected_function_result result;
            {
                const Implementation::CallScope call(
                    *implementation, implementation->callTo(actor, asked, "fact"));
                const InstructionBudget budget(implementation->lua.lua_state());
                result = fact.answer(memory, view, deltaTime);
            }
            bool holds = false;
            if (!result.valid())
            {
                implementation->report(actor, asked, "fact", resultError(result));
            }
            else if (const sol::object returned = result; returned.is<bool>())
            {
                holds = returned.as<bool>();
            }
            else
            {
                implementation->report(actor, asked, "fact", "must return true or false");
            }
            answers.emplace(fact.name, holds);
        }
        return answers;
    }

    void LuaNpcScripts::enter(
        ActorId actor,
        const NpcActivity& activity,
        const NpcActivitySnapshot& snapshot)
    {
        requireValidCall(actor, activity);
        const std::optional<sol::table> table = implementation->activityTable(activity);
        if (!table.has_value())
        {
            implementation->report(actor, activity, "enter", "activity is not loaded");
            return;
        }

        const ActivityOwner owner{actor.value, activity.script, activity.activity};
        sol::table self = implementation->lua.create_table();
        implementation->selves.insert_or_assign(owner, self);
        const sol::object hookObject = table->get<sol::object>("enter");
        if (!hookObject.valid() || hookObject.get_type() == sol::type::lua_nil)
        {
            return;
        }

        sol::protected_function hook = hookObject.as<sol::protected_function>();
        sol::protected_function_result result;
        {
            const Implementation::CallScope call(
                *implementation, implementation->callTo(actor, activity, "enter"));
            const InstructionBudget budget(implementation->lua.lua_state());
            result = hook(
                self,
                luaSnapshot(implementation->lua, snapshot),
                implementation->memoryOf(actor, activity.script));
        }
        if (!result.valid())
        {
            implementation->report(actor, activity, "enter", resultError(result));
        }
    }

    NpcActivityCommand LuaNpcScripts::update(
        ActorId actor,
        const NpcActivity& activity,
        const NpcActivitySnapshot& snapshot,
        float deltaTime)
    {
        requireValidCall(actor, activity);
        requireSeconds(deltaTime, "NPC script update time step");

        const std::optional<sol::table> table = implementation->activityTable(activity);
        if (!table.has_value())
        {
            implementation->report(actor, activity, "update", "activity is not loaded");
            return {};
        }
        const ActivityOwner owner{actor.value, activity.script, activity.activity};
        const auto self = implementation->selves.find(owner);
        if (self == implementation->selves.end())
        {
            implementation->report(actor, activity, "update", "activity was not entered");
            return {};
        }

        const sol::protected_function hook =
            table->get<sol::object>("update").as<sol::protected_function>();
        sol::protected_function_result result;
        {
            const Implementation::CallScope call(
                *implementation, implementation->callTo(actor, activity, "update"));
            const InstructionBudget budget(implementation->lua.lua_state());
            result = hook(
                self->second,
                luaSnapshot(implementation->lua, snapshot),
                deltaTime,
                implementation->memoryOf(actor, activity.script));
        }
        if (!result.valid())
        {
            implementation->report(actor, activity, "update", resultError(result));
            return {};
        }

        try
        {
            const sol::object returned = result;
            return commandFrom(returned);
        }
        catch (const std::invalid_argument& error)
        {
            implementation->report(actor, activity, "update", error.what());
            return {};
        }
    }

    void LuaNpcScripts::exit(
        ActorId actor,
        const NpcActivity& activity,
        const NpcActivitySnapshot& snapshot)
    {
        requireValidCall(actor, activity);
        const ActivityOwner owner{actor.value, activity.script, activity.activity};
        const auto self = implementation->selves.find(owner);
        if (self == implementation->selves.end())
        {
            return;
        }

        const std::optional<sol::table> table = implementation->activityTable(activity);
        if (table.has_value())
        {
            const sol::object hookObject = table->get<sol::object>("exit");
            if (hookObject.valid() && hookObject.get_type() != sol::type::lua_nil)
            {
                sol::protected_function hook = hookObject.as<sol::protected_function>();
                sol::protected_function_result result;
                {
                    const Implementation::CallScope call(
                        *implementation, implementation->callTo(actor, activity, "exit"));
                    const InstructionBudget budget(implementation->lua.lua_state());
                    result = hook(
                        self->second,
                        luaSnapshot(implementation->lua, snapshot),
                        implementation->memoryOf(actor, activity.script));
                }
                if (!result.valid())
                {
                    implementation->report(actor, activity, "exit", resultError(result));
                }
            }
        }
        implementation->selves.erase(self);
    }

    void LuaNpcScripts::forget(ActorId actor)
    {
        for (auto entry = implementation->memories.begin();
             entry != implementation->memories.end();)
        {
            if (entry->first.actor == actor.value)
            {
                entry = implementation->memories.erase(entry);
            }
            else
            {
                ++entry;
            }
        }
        for (auto entry = implementation->selves.begin(); entry != implementation->selves.end();)
        {
            if (entry->first.actor == actor.value)
            {
                entry = implementation->selves.erase(entry);
            }
            else
            {
                ++entry;
            }
        }
    }

    const std::vector<LuaScriptDiagnostic>& LuaNpcScripts::diagnostics() const
    {
        return implementation->reported;
    }

    std::vector<LuaScriptDiagnostic> LuaNpcScripts::takeDiagnostics()
    {
        return std::exchange(implementation->reported, {});
    }
}

// NOLINTEND(misc-include-cleaner)
