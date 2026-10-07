#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_message.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>

#include <glm/vec2.hpp>

#include "game.hpp"
#include "lua_npc_scripts.hpp"
#include "advanced_platformer/level/level_generator.hpp"
#include "advanced_platformer/actor/actor.hpp"
#include "advanced_platformer/input/input_state.hpp"
#include "advanced_platformer/inventory/inventory.hpp"
#include "advanced_platformer/render/sprite.hpp"
#include "support/fixed_step.hpp"
#include "support/fixture_game.hpp"

namespace
{
    constexpr int MaximumSimulationTicks = 12000;

    bool sameHealth(advanced_platformer::Health left, advanced_platformer::Health right)
    {
        return left.current == right.current && left.maximum == right.maximum;
    }

    bool sameInventory(
        const advanced_platformer::Inventory& left,
        const advanced_platformer::Inventory& right)
    {
        const auto& leftSlots = left.slots();
        const auto& rightSlots = right.slots();
        if (leftSlots.size() != rightSlots.size())
        {
            return false;
        }

        for (std::size_t slot = 0; slot < leftSlots.size(); ++slot)
        {
            const auto& leftSlot = leftSlots[slot];
            const auto& rightSlot = rightSlots[slot];
            if (leftSlot.has_value() != rightSlot.has_value())
            {
                return false;
            }
            if (leftSlot.has_value() && (leftSlot.value().item != rightSlot.value().item ||
                                         leftSlot.value().quantity != rightSlot.value().quantity))
            {
                return false;
            }
        }

        return true;
    }
}

TEST_CASE(
    "The exit leads to the next level and keeps the player's progress",
    "[app][level-transition]")
{
    advanced_platformer::Game game = tests::fixtureGame("tests/fixtures/rooms/finish/pieces.json");
    advanced_platformer::InputIntentions intentions;
    intentions.direction.x = -1.0F;

    for (int expected = 2; expected <= 3; ++expected)
    {
        for (int tick = 0; tick < MaximumSimulationTicks && game.levelNumber() < expected; ++tick)
        {
            const int previousLevel = game.levelNumber();
            const auto previousHealth = game.playerHealth();
            const auto previousInventory = game.playerInventory();
            game.update(intentions, tests::FixedStepSeconds);
            if (game.levelNumber() != previousLevel)
            {
                REQUIRE(sameHealth(game.playerHealth(), previousHealth));
                REQUIRE(sameInventory(game.playerInventory(), previousInventory));
            }
        }
        REQUIRE(game.levelNumber() == expected);
        REQUIRE(game.levelSeed() == advanced_platformer::runLevelSeed(1, expected));
        REQUIRE(game.runSeed() == 1U);
    }
}

TEST_CASE(
    "Defeat restarts the run at level 1 with a fresh player and a new run seed",
    "[app][level-transition]")
{
    advanced_platformer::GameContent content =
        tests::fixtureContent("tests/fixtures/rooms/contact_enemy/pieces.json");
    content.npcScripts.loadScript("contact_enemy", "tests/fixtures/scripts/contact_enemy.lua");
    advanced_platformer::Game game = tests::fixtureGame(std::move(content));
    const auto initialHealth = game.playerHealth();
    const auto initialInventory = game.playerInventory();
    advanced_platformer::InputIntentions intentions;
    intentions.direction.x = -1.0F;
    bool collected = false;

    for (int tick = 0; tick < MaximumSimulationTicks && game.runSeed() == 1U; ++tick)
    {
        game.update(intentions, tests::FixedStepSeconds);
        collected = collected || game.playerInventory().count(1) > 0;
    }

    REQUIRE(collected);
    const std::uint32_t runSeed = advanced_platformer::nextRunSeed(1);
    REQUIRE(game.runSeed() == runSeed);
    REQUIRE(game.levelNumber() == 1);
    REQUIRE(game.levelSeed() == advanced_platformer::runLevelSeed(runSeed, 1));
    REQUIRE(sameHealth(game.playerHealth(), initialHealth));
    REQUIRE(sameInventory(game.playerInventory(), initialInventory));
}

TEST_CASE(
    "The game hints the missing item while the player stands in a locked exit",
    "[app][level-transition][exit]")
{
    advanced_platformer::Game game =
        tests::fixtureGame("tests/fixtures/rooms/locked_door/pieces.json");
    REQUIRE_FALSE(game.lockedExitHintIcon().has_value());

    advanced_platformer::InputIntentions walkLeft;
    walkLeft.direction.x = -1.0F;
    int ticks = 0;
    while (!game.lockedExitHintIcon().has_value() && ticks < MaximumSimulationTicks)
    {
        game.update(walkLeft, tests::FixedStepSeconds);
        ++ticks;
    }
    const advanced_platformer::Sprite icon =
        game.lockedExitHintIcon().value_or(advanced_platformer::Sprite{});
    REQUIRE(icon.region.position == glm::vec2{0.0F, 0.0F});
    REQUIRE(icon.region.size == glm::vec2{8.0F, 8.0F});

    for (int tick = 0; tick < 120; ++tick)
    {
        game.update({}, tests::FixedStepSeconds);
    }
    REQUIRE(game.lockedExitHintIcon().has_value());

    for (int tick = 0; tick < 120; ++tick)
    {
        game.update(walkLeft, tests::FixedStepSeconds);
    }
    REQUIRE_FALSE(game.lockedExitHintIcon().has_value());
    REQUIRE(game.playerInventory().count(1) == 1);

    advanced_platformer::InputIntentions walkRight;
    walkRight.direction.x = 1.0F;
    const int lockedLevel = game.levelNumber();
    for (ticks = 0; ticks < MaximumSimulationTicks && game.levelNumber() == lockedLevel; ++ticks)
    {
        game.update(walkRight, tests::FixedStepSeconds);
    }
    REQUIRE(game.levelNumber() == lockedLevel + 1);
}
