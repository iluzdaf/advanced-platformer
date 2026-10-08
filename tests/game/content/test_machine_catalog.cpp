#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <stdexcept>
#include "content/content_glaze.hpp"
#include "content/machine_catalog.hpp"
#include "advanced_platformer/npc/npc_activity.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "support/json_document.hpp"

using Catch::Matchers::ContainsSubstring;

TEST_CASE("Machine JSON keeps state order, expands from lists and reads holds", "[app][machines]")
{
    auto machineJson = tests::parseJson(
        advanced_platformer::loadContentText("tests/fixtures/catalogs/machines.json"));
    auto& machine = machineJson["machines"]["test_machine"];
    machine["states"].get_array().push_back(
        tests::object(
            {{"name", "flee"},
             {"does", tests::object({{"script", "rat"}, {"activity", "flee"}})}}));
    machine["transitions"].get_array().push_back(
        tests::object(
            {{"from", tests::list({"rest", "hunt"})},
             {"to", "flee"},
             {"when", tests::object({{"targetWithinStandoffDistance", true}})}}));
    const auto catalog =
        advanced_platformer::parseMachineCatalog(tests::dumpJson(machineJson), "test machines");
    const advanced_platformer::NpcStateMachine& parsed =
        advanced_platformer::npcStateMachine(catalog, "test_machine");

    REQUIRE(parsed.name == "test_machine");
    REQUIRE(parsed.states.size() == 3);
    REQUIRE(parsed.states[0].name == "rest");
    REQUIRE(parsed.states[0].does == advanced_platformer::NpcActivity{"test", "idle"});
    REQUIRE(parsed.states[2].does == advanced_platformer::NpcActivity{"rat", "flee"});
    REQUIRE(parsed.transitions.size() == 4);
    REQUIRE(parsed.transitions[1].after == 0.5F);
    REQUIRE(parsed.transitions[1].when.at("targetKnown") == false);
    REQUIRE(parsed.transitions[2].from == "rest");
    REQUIRE(parsed.transitions[3].from == "hunt");
    REQUIRE(parsed.transitions[3].to == "flee");
    REQUIRE_THROWS_AS(
        advanced_platformer::npcStateMachine(catalog, "missing"), std::invalid_argument);
}

TEST_CASE("Machine JSON rejects what the engine cannot run, naming where", "[app][machines]")
{
    auto machineJson = tests::parseJson(
        advanced_platformer::loadContentText("tests/fixtures/catalogs/machines.json"));
    auto& machine = machineJson["machines"]["test_machine"];
    const char* expected = "";
    SECTION("An activity named by a string")
    {
        machine["states"][0]["does"] = "idle";
        expected = "expected an object, found 'idle'";
    }
    SECTION("An empty state name")
    {
        machine["states"][0]["name"] = "";
        expected = "machines.test_machine.states[0].name";
    }
    SECTION("An activity without a script")
    {
        machine["states"][0]["does"] = tests::object({{"activity", "flee"}});
        expected = "missing 'script'";
    }
    SECTION("An activity with an unknown field")
    {
        machine["states"][0]["does"]["kind"] = "lua";
        expected = "unknown field 'kind'";
    }
    SECTION("Unknown fact")
    {
        machine["transitions"][0]["when"]["cornered"] = true;
        expected = "asks about \"cornered\"";
    }
    SECTION("A transition to a state the machine lacks")
    {
        machine["transitions"][0]["to"] = "pounce";
        expected = "leads to a state the machine lacks";
    }
    SECTION("A condition that is not a boolean")
    {
        machine["transitions"][0]["when"]["targetKnown"] = 1;
        expected = "expected true or false";
    }
    SECTION("An empty from list")
    {
        machine["transitions"][0]["from"] = tests::emptyArray();
        expected = "at least one state name";
    }
    SECTION("Unknown field")
    {
        machine["start"] = "rest";
        expected = "unknown field 'start'";
    }
    REQUIRE_THROWS_WITH(
        advanced_platformer::parseMachineCatalog(tests::dumpJson(machineJson), "machines.json"),
        ContainsSubstring("machines.json") && ContainsSubstring(expected));
}
