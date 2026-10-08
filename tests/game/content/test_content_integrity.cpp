#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "content/game_catalogs.hpp"
#include "advanced_platformer/level/level_generator.hpp"
#include "advanced_platformer/level/room_pieces.hpp"
#include "content/room_pieces.hpp"
#include "content/npc_script_catalog.hpp"
#include "level/level_composition.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_activity_scripts.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "lua_npc_scripts.hpp"
#include "lua_presentation_script.hpp"
#include "support/fixed_step.hpp"

namespace
{
    int levelsUntilCap(const advanced_platformer::RunSettings& run)
    {
        if (run.roomsPerLevel <= 0 || run.firstRooms >= run.maxRooms)
        {
            return 1;
        }
        const int growth = run.maxRooms - run.firstRooms;
        return 1 + ((growth + run.roomsPerLevel - 1) / run.roomsPerLevel);
    }
}

TEST_CASE("Every run level can be composed until the rooms stop growing", "[app][content]")
{
    const auto pieces = advanced_platformer::loadRoomPieceCatalog("assets/catalogs/pieces.json");
    const auto catalogs = advanced_platformer::loadGameCatalogs("assets/catalogs");
    for (int number = 1; number <= levelsUntilCap(pieces.run); ++number)
    {
        const auto content = advanced_platformer::composeLevel(
            pieces,
            number,
            advanced_platformer::runLevelSeed(1, number),
            0,
            catalogs,
            advanced_platformer::composePlayer(catalogs, 0));
        INFO("Level " << number);
        REQUIRE(content.number == number);
        REQUIRE(content.world.exit().has_value());
    }
}

TEST_CASE(
    "Every shipped room piece places its actors and pickups where they can be",
    "[app][content]")
{
    REQUIRE_NOTHROW(
        advanced_platformer::validateRoomPieces(
            advanced_platformer::loadGameCatalogs("assets/catalogs"), tests::FixedStepSeconds));
}

TEST_CASE("Every shipped Lua activity resolves", "[app][content][lua]")
{
    const advanced_platformer::GameCatalogs catalogs =
        advanced_platformer::loadGameCatalogs("assets/catalogs");
    advanced_platformer::LuaNpcScripts scripts;

    REQUIRE_NOTHROW(
        advanced_platformer::loadNpcActivityScripts(scripts, catalogs.machines, "assets/scripts"));
}

TEST_CASE("Every shipped Lua activity runs without errors", "[app][content][lua]")
{
    const advanced_platformer::GameCatalogs catalogs =
        advanced_platformer::loadGameCatalogs("assets/catalogs");
    advanced_platformer::LuaNpcScripts scripts;
    advanced_platformer::loadNpcActivityScripts(scripts, catalogs.machines, "assets/scripts");

    std::vector<advanced_platformer::NpcActivitySnapshot> situations(4);
    for (advanced_platformer::NpcActivitySnapshot& snapshot : situations)
    {
        snapshot.feet = {40.0F, 80.0F};
        snapshot.facts.primaryReady = true;
    }
    situations[0].targetFeet = {{60.0F, 80.0F}};
    situations[0].patrol = advanced_platformer::Patrol{{24.0F, 80.0F}, {96.0F, 80.0F}, true};
    situations[0].facts.targetKnown = true;
    situations[0].facts.targetVisible = true;
    situations[0].facts.targetInPrimaryRange = true;
    situations[1].patrol = situations[0].patrol;
    situations[2].routeComplete = true;
    situations[2].patrol = situations[0].patrol;
    situations[3].facts.primaryReady = false;
    situations[3].targetFeet = situations[0].targetFeet;

    std::uint32_t nextActor = 1;
    for (const auto& [machineName, machine] : catalogs.machines)
    {
        for (const advanced_platformer::NpcMachineState& state : machine.states)
        {
            for (const advanced_platformer::NpcActivitySnapshot& snapshot : situations)
            {
                const advanced_platformer::ActorId actor{nextActor++};
                scripts.enter(actor, state.does, snapshot);
                scripts.update(actor, state.does, snapshot, 1.0F / 120.0F);
                scripts.exit(actor, state.does, snapshot);
            }
            INFO(machineName << " " << state.name);
            REQUIRE(scripts.diagnostics().empty());
        }
    }
}

TEST_CASE("The shipped presentation script loads", "[app][content][lua]")
{
    advanced_platformer::LuaPresentationScript presentation;

    REQUIRE_NOTHROW(presentation.loadScript("assets/scripts/presentation.lua"));
    REQUIRE(presentation.loaded());
}
