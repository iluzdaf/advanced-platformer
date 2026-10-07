#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <sstream>
#include <stdexcept>

#include "diagnostics/console_log.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "lua_script_diagnostic.hpp"

using advanced_platformer::ConsoleLevel;
using advanced_platformer::ConsoleLog;
using advanced_platformer::LuaScriptDiagnostic;
using advanced_platformer::LuaScriptDiagnosticKind;

TEST_CASE("The console echoes each line and keeps only the latest", "[app][debug][console]")
{
    std::ostringstream echo;
    ConsoleLog log(echo, 2);

    log.write(ConsoleLevel::Info, "first");
    log.write(ConsoleLevel::Error, "second");
    log.write(ConsoleLevel::Info, "third");

    REQUIRE(echo.str() == "first\nsecond\nthird\n");
    REQUIRE(log.written() == 3);
    REQUIRE(log.entries().size() == 2);
    REQUIRE(log.entries().front().text == "second");
    REQUIRE(log.entries().front().level == ConsoleLevel::Error);
    REQUIRE(log.entries().back().text == "third");
    REQUIRE_THROWS_AS(ConsoleLog(echo, 0), std::invalid_argument);
}

TEST_CASE(
    "Script errors and prints reach the console with where they came from",
    "[app][debug][console]")
{
    std::ostringstream echo;
    ConsoleLog log(echo);
    LuaScriptDiagnostic error{
        "rat.lua", "rat", "flee", "update", advanced_platformer::ActorId{7}, "boom"};
    LuaScriptDiagnostic printed{"rat.lua", "rat", "", "load", std::nullopt, "loaded"};
    printed.kind = LuaScriptDiagnosticKind::Print;

    advanced_platformer::writeScriptDiagnostic(log, error);
    advanced_platformer::writeScriptDiagnostic(log, printed);

    REQUIRE(log.entries()[0].level == ConsoleLevel::Error);
    REQUIRE(log.entries()[0].text == "[lua] rat.flee update, NPC 7 error: boom");
    REQUIRE(log.entries()[1].level == ConsoleLevel::Info);
    REQUIRE(log.entries()[1].text == "[lua] rat load: loaded");
}
