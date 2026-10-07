#include <catch2/catch_test_macros.hpp>

#include <array>
#include <utility>

#include "application_context.hpp"
#include "debug/debug_tool_visibility.hpp"
#include "game/level_requests.hpp"
#include "game/play_control.hpp"

namespace
{
    using advanced_platformer::ApplicationCommand;
    using advanced_platformer::ApplicationContext;
    using advanced_platformer::DebugToolVisibility;

    constexpr std::array<std::pair<ApplicationCommand, bool DebugToolVisibility::*>, 6>
        DebugToolToggles = {{
            {ApplicationCommand::ToggleFrameProfileDetails,
             &DebugToolVisibility::frameProfileDetails},
            {ApplicationCommand::ToggleWorldAndCameraOverlay,
             &DebugToolVisibility::worldAndCameraOverlay},
            {ApplicationCommand::ToggleActorText, &DebugToolVisibility::actorText},
            {ApplicationCommand::ToggleNavigationCacheText,
             &DebugToolVisibility::navigationCacheText},
            {ApplicationCommand::ToggleStateMachine, &DebugToolVisibility::stateMachine},
            {ApplicationCommand::ToggleConsole, &DebugToolVisibility::console},
        }};
}

TEST_CASE("Play commands reach the play control", "[app][commands]")
{
    ApplicationContext context;

    applyCommand(context, ApplicationCommand::ToggleInventory);

    REQUIRE(context.play.inventoryOpen());

    applyCommand(context, ApplicationCommand::ToggleInventory);
    applyCommand(context, ApplicationCommand::TogglePause);

    REQUIRE_FALSE(context.play.inventoryOpen());
    REQUIRE(context.play.simulationPaused());

    applyCommand(context, ApplicationCommand::TogglePause);

    REQUIRE_FALSE(context.play.simulationPaused());
}

TEST_CASE("Level commands raise one request each", "[app][commands]")
{
    ApplicationContext context;

    applyCommand(context, ApplicationCommand::RestartLevel);

    REQUIRE(context.levelRequests.restartLevel);
    REQUIRE_FALSE(context.levelRequests.rerollLevel);

    applyCommand(context, ApplicationCommand::RerollLevel);

    REQUIRE(context.levelRequests.rerollLevel);
}

TEST_CASE("Quit raises the quit request", "[app][commands]")
{
    ApplicationContext context;

    applyCommand(context, ApplicationCommand::Quit);

    REQUIRE(context.quitRequested);
}

TEST_CASE("The debug overlay toggles and steps through bodies", "[app][commands]")
{
    ApplicationContext context;

    applyCommand(context, ApplicationCommand::ToggleDebugOverlay);
    applyCommand(context, ApplicationCommand::NextDebugBody);
    applyCommand(context, ApplicationCommand::NextDebugBody);

    REQUIRE(context.showDebugOverlay);
    REQUIRE(context.debugBodyIndex == 2);

    applyCommand(context, ApplicationCommand::ToggleDebugOverlay);

    REQUIRE_FALSE(context.showDebugOverlay);
}

TEST_CASE("Debug tools toggle only while the overlay is shown", "[app][commands]")
{
    ApplicationContext context;

    for (const auto& [command, tool] : DebugToolToggles)
    {
        applyCommand(context, command);

        REQUIRE_FALSE(context.debugToolVisibility.*tool);
    }

    applyCommand(context, ApplicationCommand::ToggleDebugOverlay);
    for (const auto& [command, tool] : DebugToolToggles)
    {
        applyCommand(context, command);

        REQUIRE(context.debugToolVisibility.*tool);

        applyCommand(context, command);

        REQUIRE_FALSE(context.debugToolVisibility.*tool);
    }
}

TEST_CASE("Breaking a tile is requested only while the overlay is shown", "[app][commands]")
{
    ApplicationContext context;

    applyCommand(context, ApplicationCommand::BreakTile);

    REQUIRE_FALSE(context.debugCursor.breakTileRequested);

    applyCommand(context, ApplicationCommand::ToggleDebugOverlay);
    applyCommand(context, ApplicationCommand::BreakTile);

    REQUIRE(context.debugCursor.breakTileRequested);
}
