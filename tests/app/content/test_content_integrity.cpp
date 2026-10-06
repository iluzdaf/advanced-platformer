#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "content/game_catalogs.hpp"
#include "content/level_catalog.hpp"
#include "content/npc_script_catalog.hpp"
#include "game/level_composition.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/actor/actor_id.hpp"
#include "advanced_platformer/math/aabb.hpp"
#include "advanced_platformer/npc/npc.hpp"
#include "advanced_platformer/npc/npc_activity_scripts.hpp"
#include "advanced_platformer/npc/npc_state_machine.hpp"
#include "lua_npc_scripts.hpp"
#include "lua_presentation_script.hpp"
#include "advanced_platformer/world/level_exit.hpp"
#include "advanced_platformer/world/level_validation.hpp"
#include "support/add_player.hpp"

namespace
{
    constexpr const char* ShippedAtlas = "assets/textures/sprites.png";

    glm::ivec2 pngSize(const char* path)
    {
        std::ifstream file(path, std::ios::binary);
        std::array<unsigned char, 24> header{};
        file.read(reinterpret_cast<char*>(header.data()), header.size());
        REQUIRE(file.gcount() == static_cast<std::streamsize>(header.size()));
        const auto word = [&header](std::size_t at)
        {
            return static_cast<int>(
                (static_cast<std::uint32_t>(header[at]) << 24U) |
                (static_cast<std::uint32_t>(header[at + 1]) << 16U) |
                (static_cast<std::uint32_t>(header[at + 2]) << 8U) |
                static_cast<std::uint32_t>(header[at + 3]));
        };
        return {word(16), word(20)};
    }
}

TEST_CASE("Every catalog region lies inside the shipped atlas", "[app][content][atlas]")
{
    const glm::ivec2 atlas = pngSize(ShippedAtlas);
    REQUIRE_NOTHROW(advanced_platformer::loadGameCatalogs("assets/catalogs", atlas));
}

TEST_CASE("Every catalog level can be composed", "[app][content]")
{
    const auto catalog = advanced_platformer::loadLevelCatalog("assets/levels/levels.json");
    const auto catalogs =
        advanced_platformer::loadGameCatalogs("assets/catalogs", pngSize(ShippedAtlas));

    REQUIRE_FALSE(catalog.levels.empty());
    for (const advanced_platformer::LevelCatalogEntry& entry : catalog.levels)
    {
        const auto content =
            advanced_platformer::composeGameLevel(catalog, entry.number, 0, catalogs);
        REQUIRE(content.number == entry.number);

        const auto& levelExit = content.world.exit();
        if (!levelExit.has_value())
        {
            throw std::logic_error("A composed level must have an exit");
        }
        const advanced_platformer::LevelExit& exit = levelExit.value();
        if (exit.nextLevel.has_value())
        {
            REQUIRE_NOTHROW(advanced_platformer::levelEntry(catalog, exit.nextLevel.value()));
        }
    }
}

TEST_CASE("Every catalog level has valid actor placement", "[app][content]")
{
    const auto catalog = advanced_platformer::loadLevelCatalog("assets/levels/levels.json");
    const auto catalogs =
        advanced_platformer::loadGameCatalogs("assets/catalogs", pngSize(ShippedAtlas));
    for (const advanced_platformer::LevelCatalogEntry& entry : catalog.levels)
    {
        auto content = advanced_platformer::composeGameLevel(catalog, entry.number, 0, catalogs);
        advanced_platformer::Actor player = advanced_platformer::composePlayer(catalogs, 0);
        advanced_platformer::moveFeetTo(player.body.bounds, content.playerSpawnFeet);
        tests::addPlayer(content.world, player);

        REQUIRE_NOTHROW(
            advanced_platformer::validateLevelActors(content.map, content.world, content.number));
    }
}

TEST_CASE("Every shipped Lua activity resolves", "[app][content][lua]")
{
    const advanced_platformer::GameCatalogs catalogs =
        advanced_platformer::loadGameCatalogs("assets/catalogs", pngSize(ShippedAtlas));
    advanced_platformer::LuaNpcScripts scripts;

    REQUIRE_NOTHROW(
        advanced_platformer::loadNpcActivityScripts(scripts, catalogs.machines, "assets/scripts"));
}

TEST_CASE("The shipped presentation script loads", "[app][content][lua]")
{
    advanced_platformer::LuaPresentationScript presentation;

    REQUIRE_NOTHROW(presentation.loadScript("assets/scripts/presentation.lua"));
    REQUIRE(presentation.loaded());
}

TEST_CASE("Every shipped Lua activity runs without errors", "[app][content][lua]")
{
    const advanced_platformer::GameCatalogs catalogs =
        advanced_platformer::loadGameCatalogs("assets/catalogs", pngSize(ShippedAtlas));
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
